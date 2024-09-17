#ifndef SCREEN_H
#define SCREEN_H

class Screen {
    public:
        lv_obj_t *scr; 

        void on_swipe_up() {}
        void on_swipe_down() {}
        void on_tick(uint32_t tick) {}
};

#endif
