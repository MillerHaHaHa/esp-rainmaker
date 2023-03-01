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

/**
 * @brief touchpad once trigger pressed callback
 * 
 * @see 
 * @warning 
 * @note 100 ms once
 */
typedef void (*app_touch_pad_press_start_cb)(uint8_t num);

/**
 * @brief touchpad once trigger released callback
 * 
 * @see 
 * @warning 
 * @note 
 */
typedef void (*app_touch_pad_press_end_cb)(uint8_t num, uint32_t press_time_ms);

/**
 * @brief touchpad keep activated callback
 * 
 * @see 
 * @warning 
 * @note 100 ms once
 */
typedef void (*app_touch_pad_press_keep_cb)(uint8_t num, uint32_t keep_time_ms);

typedef struct {
    app_touch_pad_press_start_cb    press_start_cb;
    app_touch_pad_press_end_cb      press_end_cb;
    app_touch_pad_press_keep_cb     press_keep_cb;
} app_touch_pad_cfg_t;

void app_touch_pad_init(app_touch_pad_cfg_t *pCfg);
