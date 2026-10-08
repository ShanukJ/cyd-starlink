#include "UiManager.h"

#include <Arduino.h>

#include "Theme.h"

namespace ui {

namespace {
constexpr uint32_t kUpdateMs = 250;
constexpr uint32_t kSlideMs = 180;
constexpr int32_t kDotSize = 6;
}  // namespace

void UiManager::begin() {
    _dashboard.build(onOpenHardwareTest, this);
    _alignment.build();
    _diagnostics.build(_board.profile().name);
    _test.build(onCloseHardwareTest, this);
    _setup.build(onSetupClose, this);
    for (int i = 0; i < kPageCount; ++i) lv_obj_add_event_cb(pageScreen(i), onGesture, LV_EVENT_GESTURE, this);
    buildPageDots();

    lv_screen_load(_dashboard.screen());
    update();
    lv_timer_create(onTimer, kUpdateMs, this);
}

lv_obj_t* UiManager::pageScreen(int page) const {
    switch (page) {
        case 1: return _alignment.screen();
        case 2: return _diagnostics.screen();
        default: return _dashboard.screen();
    }
}

void UiManager::update() {
    const net::WifiStatus wifi = _wifi.status();
    const starlink::StarlinkSnapshot starlink = _starlink.snapshot();

    // Keep every screen current (cheap: labels only change when text does),
    // so switching screens never shows stale content.
    _dashboard.update(starlink, millis());
    _alignment.update(starlink);
    _diagnostics.update(starlink, wifi);
    _test.setWifiStatus(wifi);
    _test.setStarlink(starlink);
    if (wifi.portalActive) _setup.update(wifi);

    lv_obj_t* wanted = _hardwareTest ? _test.screen() : pageScreen(_page);
    if (wifi.portalActive) wanted = _setup.screen();
    showPageDots(wanted == pageScreen(_page));

    // Don't interrupt a slide animation that is already heading there.
    if (lv_screen_active() != wanted && lv_display_get_screen_loading(nullptr) != wanted) {
        lv_screen_load(wanted);
    }

    for (int i = 0; i < kPageCount; ++i) {
        lv_obj_set_style_bg_opa(_dot[i], i == _page ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

void UiManager::goToPage(int page, lv_screen_load_anim_t anim) {
    page = (page + kPageCount) % kPageCount;
    if (page == _page) return;
    _page = page;
    lv_screen_load_anim(pageScreen(page), anim, kSlideMs, 0, false);
    update();
}

void UiManager::onGesture(lv_event_t* e) {
    auto* self = static_cast<UiManager*>(lv_event_get_user_data(e));
    const lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    // Finger moves left => next page slides in from the right.
    if (dir == LV_DIR_LEFT) self->goToPage(self->_page + 1, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    if (dir == LV_DIR_RIGHT) self->goToPage(self->_page - 1, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

void UiManager::onDotsClicked(lv_event_t* e) {
    auto* self = static_cast<UiManager*>(lv_event_get_user_data(e));
    self->goToPage(self->_page + 1, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
}

void UiManager::buildPageDots() {
    // On the top layer so one indicator serves every page. Also a tap
    // target (next page) for when swiping is awkward.
    _dots = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_dots);
    lv_obj_set_size(_dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(_dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(_dots, 6, 0);
    lv_obj_set_style_pad_all(_dots, 4, 0);
    lv_obj_align(_dots, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_ext_click_area(_dots, 14);
    lv_obj_add_event_cb(_dots, onDotsClicked, LV_EVENT_CLICKED, this);
    for (int i = 0; i < kPageCount; ++i) {
        _dot[i] = lv_obj_create(_dots);
        lv_obj_remove_style_all(_dot[i]);
        lv_obj_set_size(_dot[i], kDotSize, kDotSize);
        lv_obj_set_style_radius(_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(_dot[i], lv_color_hex(theme::kMuted), 0);
        lv_obj_remove_flag(_dot[i], LV_OBJ_FLAG_CLICKABLE);
    }
}

void UiManager::showPageDots(bool visible) {
    if (visible == lv_obj_has_flag(_dots, LV_OBJ_FLAG_HIDDEN)) {
        if (visible) {
            lv_obj_remove_flag(_dots, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_dots, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void UiManager::onTimer(lv_timer_t* timer) { static_cast<UiManager*>(lv_timer_get_user_data(timer))->update(); }

void UiManager::onSetupClose(void* ctx) { static_cast<UiManager*>(ctx)->_wifi.requestPortalClose(); }

void UiManager::onOpenHardwareTest(void* ctx) {
    auto* self = static_cast<UiManager*>(ctx);
    self->_hardwareTest = true;
    self->update();
}

void UiManager::onCloseHardwareTest(void* ctx) {
    auto* self = static_cast<UiManager*>(ctx);
    self->_hardwareTest = false;
    self->update();
}

}  // namespace ui
