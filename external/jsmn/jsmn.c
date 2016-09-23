#include "jsmn.h"

#include <assert.h>

typedef struct {
	jsmntype_t type;
	int start;
	int end;
	int size;
	int skip;
	int parent;
	/* private data */
	int _lasttype;
	int _depth;
} jsmntok_priv_t;

/**
 * Private parsing tokens.
 */
enum {
	_JSMN_SEPARATOR_COLON = 201,
	_JSMN_SEPARATOR_COMMA,
	_JSMN_OBJECT_END,
	_JSMN_ARRAY_END,
	_JSMN_STRING_KEY,
};

/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
		jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
	tok->skip = 1;
	tok->parent = -1;
	((jsmntok_priv_t *)tok)->_lasttype = JSMN_UNDEFINED;
	((jsmntok_priv_t *)tok)->_depth = -1;
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/**
 * Parse digit(s).
 */
static int jsmn_parse_primitive_digits(const char *js, jsmntok_t *token, int start, int *end) {
	int i;
	int ended = 0;

	/* must be at least one digit */
	i = start;
	switch (js[i]) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			break;
		default:
			return JSMN_ERROR_PRIMITIVE_INVAL;
	}
	++i;

	for (; i < token->end; i++) {
		switch (js[i]) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				break;
			default:
				ended = 1;
				break;
		}

		if (ended) {
			break;
		}
	}

	*end = i;
	return 0;
}

/**
 * Parse exponent.
 */
static int jsmn_parse_primitive_exp(const char *js, jsmntok_t *token, int start, int *end) {
	int i;
	int ret;

	i = start;
	assert(i < token->end);

	if (js[i] != 'e' && js[i] != 'E') {
		return JSMN_ERROR_PRIMITIVE_INVAL;
	}

	++i;
	if (i >= token->end) {
		return JSMN_ERROR_PART;
	}

	if (js[i] == '-' || js[i] == '+') {
		++i;
		if (i >= token->end) {
			return JSMN_ERROR_PART;
		}
	}

	ret = jsmn_parse_primitive_digits(js, token, i, &i);
	if (ret) {
		return ret;
	}

	/* should be exactly at the end */
	if (i != token->end) {
		return JSMN_ERROR_PRIMITIVE_INVAL;
	}

	*end = i;
	return 0;
}

/**
 * Parse fraction or exponent component.
 */
static int jsmn_parse_primitive_frac_or_exp(const char *js, jsmntok_t *token, int start, int *end) {
	int i;
	int ret;

	i = start;
	assert(i < token->end);

	if (js[i] == '.') {
		++i;
		if (i >= token->end) {
			return JSMN_ERROR_PART;
		}
		ret = jsmn_parse_primitive_digits(js, token, i, &i);
		if (ret) {
			return ret;
		}
		if (i < token->end) {
			ret = jsmn_parse_primitive_exp(js, token, i, &i);
			if (ret) {
				return ret;
			}
		}
	} else {
		ret = jsmn_parse_primitive_exp(js, token, i, &i);
		if (ret) {
			return ret;
		}
	}

	*end = i;
	return 0;
}

/**
 * Parse primitive number.
 */
static int jsmn_parse_primitive_number(const char *js, jsmntok_t *token) {
	int i;
	int ret;

	i = token->start;

	if (token->end - token->start <= 0) {
		return JSMN_ERROR_PRIMITIVE_INVAL;
	}

	/* parse negative */
	if (js[i] == '-') {
		++i;
		if (i >= token->end) {
			return JSMN_ERROR_PART;
		}
	}

	/* parse beginning zero */
	if (js[i] == '0') {
		++i;
		if (i < token->end) {
			ret = jsmn_parse_primitive_frac_or_exp(js, token, i, &i);
			if (ret) {
				return ret;
			}
			if (i < token->end) {
				return JSMN_ERROR_PRIMITIVE_INVAL;
			}
		}
		return 0;
	}

	/* parse main digits */
	ret = jsmn_parse_primitive_digits(js, token, i, &i);
	if (ret) {
		return ret;
	}
	if (i == token->end) {
		return 0;
	}

	/* parse remaining fraction or exponent */
	assert(i < token->end);
	ret = jsmn_parse_primitive_frac_or_exp(js, token, i, &i);

	if (i < token->end) {
		return JSMN_ERROR_PRIMITIVE_INVAL;
	}

	return ret;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
		size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;
	int start;

	start = parser->pos;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}
	if ( (parser->pos < len) && (js[parser->pos] != '\0') ) {
		/* In strict mode primitive must be followed by a comma/object/array or end-of-string */
		parser->pos = start;
		return JSMN_ERROR_PART;
	}

found:
	token = jsmn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}
	jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
	token->parent = parser->toksuper;

	/* parse/validate primitives according */
	do {
		if (js[start] == 't') {
			if ( (token->end - start == 4) && (js[start+1] == 'r') && (js[start+2] == 'u') && (js[start+3] == 'e') ) {
				break;
			}
		} else if (js[start] == 'f') {
			if ( (token->end - start == 5) && (js[start+1] == 'a') && (js[start+2] == 'l') && (js[start+3] == 's') && (js[start+4] == 'e') ) {
				break;
			}
		} else if (js[start] == 'n') {
			if ( (token->end - start == 4) && (js[start+1] == 'u') && (js[start+2] == 'l') && (js[start+3] == 'l') ) {
				break;
			}
		} else {
			if (jsmn_parse_primitive_number(js, token) == 0) {
				break;
			}
		}

		/* primitive validation failed */
		parser->pos = token->start;
		return JSMN_ERROR_PRIMITIVE_INVAL;
	} while (0);

	parser->pos--;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
		size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;

	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSMN_ERROR_NOMEM;
			}
			jsmn_fill_token(token, JSMN_STRING, start+1, parser->pos);
			token->parent = parser->toksuper;
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (js[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;
					for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
									(js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
									(js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
							parser->pos = start;
							return JSMN_ERROR_INVAL;
						}
						parser->pos++;
					}
					parser->pos--;
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSMN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSMN_ERROR_PART;
}

static void jsmn_percolate_skip_counts(jsmntok_t *tokens, int parent) {
	jsmntok_t *token;

	token = &tokens[parent];
	token->skip++;
	for (;;) {
		if (token->parent == -1) {
			break;
		}
		token = &tokens[token->parent];
		token->skip++;
	}
}

/**
 * Parse JSON string and fill tokens.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens) {
	int r;
	int i;
	jsmntok_priv_t *token;
	int depth = 0;
	int count = parser->toknext;

	if (tokens == NULL) {
		return JSMN_ERROR_NOMEM;
	}

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c;
		jsmntype_t type;

		c = js[parser->pos];
		switch (c) {
			case '{': case '[':
				/* check previous token is valid */
				if (parser->toknext >= 1) {
					token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
					depth = token->_depth;
					switch (token->_lasttype) {
						case _JSMN_SEPARATOR_COMMA:
						case _JSMN_SEPARATOR_COLON:
						case JSMN_ARRAY:
							break;
						default:
							return JSMN_ERROR_INVAL;
					}
				}
				/* check parent token is valid */
				if (parser->toksuper != -1) {
					token = (jsmntok_priv_t *)&tokens[parser->toksuper];
					if (token->type == JSMN_OBJECT) {
						return JSMN_ERROR_INVAL;
					}
				}
				count++;
				token = (jsmntok_priv_t *)jsmn_alloc_token(parser, tokens, num_tokens);
				if (token == NULL) return JSMN_ERROR_NOMEM;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
					token->parent = parser->toksuper;
					jsmn_percolate_skip_counts(tokens, parser->toksuper);
				}
				token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
				token->_lasttype = token->type;
				token->_depth = depth+1;
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
				break;
			case '}': case ']':
				type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
				if (parser->toksuper == -1) {
					return JSMN_ERROR_INVAL;
				}
				if (parser->toknext < 1) {
					return JSMN_ERROR_INVAL;
				}
				token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
				switch (token->_lasttype) {
					case JSMN_PRIMITIVE:
					case JSMN_STRING:
					case JSMN_OBJECT:
					case JSMN_ARRAY:
					case _JSMN_OBJECT_END:
					case _JSMN_ARRAY_END:
						break;
					default:
						return JSMN_ERROR_INVAL;
				}
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSMN_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
						token->_depth--;
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = (jsmntok_priv_t *)&tokens[token->parent];
				}
				token->_lasttype = token->type == JSMN_OBJECT ? _JSMN_OBJECT_END : _JSMN_ARRAY_END;
				break;
			case '\"':
				if (parser->toknext >= 1) {
					token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
					depth = token->_depth;
					switch (token->_lasttype) {
						case JSMN_OBJECT:
						case JSMN_ARRAY:
						case _JSMN_SEPARATOR_COLON:
						case _JSMN_SEPARATOR_COMMA:
							break;
						default:
							return JSMN_ERROR_INVAL;
					}
				}
				r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
					jsmn_percolate_skip_counts(tokens, parser->toksuper);
					token = (jsmntok_priv_t *)&tokens[parser->toksuper];
					type = token->_lasttype == JSMN_OBJECT ? _JSMN_STRING_KEY : JSMN_STRING;
				} else {
				    type = JSMN_STRING;
				}
				token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
				token->_lasttype = type;
				token->_depth = depth;
				break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
			case ':':
				if (parser->toksuper == -1) {
					return JSMN_ERROR_INVAL;
				}
				assert(parser->toknext >= 1);
				token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
				switch (token->_lasttype) {
					case _JSMN_STRING_KEY:
						break;
					default:
						return JSMN_ERROR_INVAL;
				}
				parser->toksuper = parser->toknext - 1;
				token->_lasttype = _JSMN_SEPARATOR_COLON;
				break;
			case ',':
				if (parser->toksuper == -1) {
					return JSMN_ERROR_INVAL;
				}
				assert(parser->toknext >= 1);
				token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
				switch (token->_lasttype) {
					case JSMN_PRIMITIVE:
					case JSMN_STRING:
					case _JSMN_OBJECT_END:
					case _JSMN_ARRAY_END:
						break;
					default:
						return JSMN_ERROR_INVAL;
				}
				if (
						tokens[parser->toksuper].type != JSMN_ARRAY &&
						tokens[parser->toksuper].type != JSMN_OBJECT) {
					parser->toksuper = tokens[parser->toksuper].parent;
				}
				token->_lasttype = _JSMN_SEPARATOR_COMMA;
				break;
			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				/* And they must not be keys of the object */
				if (parser->toksuper != -1) {
					token = (jsmntok_priv_t *)&tokens[parser->toksuper];
					if (token->type == JSMN_OBJECT ||
							(token->type == JSMN_STRING && token->size != 0)) {
						return JSMN_ERROR_INVAL;
					}
					depth = token->_depth;
				}
				if (parser->toknext >= 1) {
					token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
					switch (token->_lasttype) {
						case JSMN_ARRAY:
						case _JSMN_SEPARATOR_COLON:
						case _JSMN_SEPARATOR_COMMA:
							break;
						default:
							return JSMN_ERROR_INVAL;
					}
				}
				r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
					jsmn_percolate_skip_counts(tokens, parser->toksuper);
				}
				token = (jsmntok_priv_t *)&tokens[parser->toknext - 1];
				token->_lasttype = JSMN_PRIMITIVE;
				token->_depth = depth;
				break;

			/* Unexpected char in strict mode */
			default:
				return JSMN_ERROR_INVAL;
		}
	}

	if (tokens[0].type == JSMN_OBJECT || tokens[0].type == JSMN_ARRAY) {
		/* Unmatched opened object or array */
		if (((jsmntok_priv_t *)(&tokens[0]))->_depth != 0) {
				return JSMN_ERROR_INVAL;
		}
	}

#ifndef NDEBUG
	/* sanity checks */
	for (i = 0; i < parser->toknext; i++) {
		assert(tokens[i].start != -1);
		assert(tokens[i].end != -1);
		assert(((jsmntok_priv_t *)(&tokens[i]))->_depth != -1);
		assert(((jsmntok_priv_t *)(&tokens[i]))->_lasttype != JSMN_UNDEFINED);
	}
#endif

	return count;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
void jsmn_init(jsmn_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

