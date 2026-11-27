// =====================================================================
//  app/display_task.h — Task FreeRTOS vẽ panel (chạy trên core 1)
//
//  Mỗi FRAME_MS:
//    1. Cài đặt / thông báo thay đổi?  -> screen_load()
//    2. screen_tick()                   -> có gì thay đổi không?
//    3. Có -> screen_draw() + panel_present()
// =====================================================================
#pragma once

void display_task_start(void);
