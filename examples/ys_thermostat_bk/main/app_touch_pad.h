/**
 * @file app_touch_pad.h
 * @author huangmingle (lelee123a@163.com)
 * @brief 
 * @version 1.0
 * @date 2022-12-30
 * 
 * @copyright Copyright (c) 2022 Leedarson All Rights Reserved.
 * 
 */
#pragma once

#include <stdint.h>

typedef void (*app_touch_pad_press_cb)(uint8_t num, uint32_t press_time);

typedef struct {
    app_touch_pad_press_cb press_cb;
} app_touch_pad_cfg_t;

void app_touch_pad_init(app_touch_pad_cfg_t *pCfg);
