/*
** FT2 Plugin - Volume Adjustment Modal Panel API
** Applies linear volume ramp to sample data (-200% to +200%).
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Panel visibility */
void ft2_volume_panel_show(struct ft2_instance_t *inst);
void ft2_volume_panel_hide(void);
bool ft2_volume_panel_is_active(void);
void ft2_volume_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Actions */
void ft2_volume_panel_apply(void);
void ft2_volume_panel_find_max_scale(void);

/* Volume accessors */
double ft2_volume_panel_get_start_vol(void);
void ft2_volume_panel_set_start_vol(double vol);
double ft2_volume_panel_get_end_vol(void);
void ft2_volume_panel_set_end_vol(double vol);

#ifdef __cplusplus
}
#endif
