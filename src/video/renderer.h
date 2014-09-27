/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

// glvideo -- Created by Aaron Culliney

void video_driver_init(void *context);
void video_driver_main_loop(void);
void video_driver_reshape(int width, int height);
void video_driver_render(void);
void video_driver_shutdown(void);
