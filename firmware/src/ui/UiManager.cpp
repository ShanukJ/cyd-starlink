#include "UiManager.h"

#include <Arduino.h>

#include "Theme.h"

namespace ui {

namespace {
constexpr uint32_t kUpdateMs = 250;
constexpr uint32_t kSlideMs = 180;
constexpr int32_t kDotSize = 6;

bool animating() { return lv_display_get_screen_loading(nullptr) != nullptr; }
}  // namespace

void UiManager::begin() {
    buildPageDots();
    for (int i = 0; i < kPageCount; ++i) {
        lv_obj_set_style_bg_color(_dot[i], lv_color_hex(theme::kMuted), 0);
    }
    update();
    lv_timer_create(onTimer, kUpdateMs, this);
}

lv_obj_t* UiManager::builtScreen(View view, int page) const {
    switch (view) {
        case View::Setup: return _setup.screen();
        case View::HardwareTest: return _test.screen();
        case View::Page: break;
    }
    switch (page) {
        case 1: return _history.screen();
        case 2: return _alignment.screen();
        case 3: return _diagnostics.screen();
        default: return _dashboard.screen();
    }
}

lv_obj_t* UiManager::buildScreen(View view, int page) {
    if (lv_obj_t* s = builtScreen(view, page)) return s;
    switch (view) {
        case View::Setup: _setup.build(onSetupClose, this); break;
        case View::HardwareTest: _test.build(onCloseHardwareTest, this); break;
        case View::Page:
            switch (page) {
                case 1: _history.build(); break;
                case 2: _alignment.build(); break;
                case 3: _diagnostics.build(_board.profile().name); break;
                default: _dashboard.build(onOpenHardwareTest, this); break;
            }
            lv_obj_add_event_cb(builtScreen(view, page), onGesture, LV_EVENT_GESTURE, this);
            break;
    }
    return builtScreen(view, page);
}

void UiManager::update() {
    const net::WifiStatus wifi = _wifi.status();
    const starlink::StarlinkSnapshot starlink = _starlink.snapshot();

    const View view = wifi.portalActive ? View::Setup : _hardwareTest ? View::HardwareTest : View::Page;
    if (!animating()) {
        lv_obj_t* wanted = builtScreen(view, _page);
        if (!wanted || lv_screen_active() != wanted) {
            // Instant switch; the previous screen is deleted.
            lv_screen_load_anim(buildScreen(view, _page), LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
        }
    }
    showPageDots(view == View::Page);
    for (int i = 0; i < kPageCount; ++i) {
        lv_obj_set_style_bg_opa(_dot[i], i == _page ? LV_OPA_COVER : LV_OPA_40, 0);
    }

    // Each screen ignores updates while it isn't built (but keeps any
    // non-visual state, e.g. history and the DEGRADED hold).
    _dashboard.update(starlink, millis());
    _history.update(_starlink, millis());
    _alignment.update(starlink);
    _diagnostics.update(starlink, wifi);
    _test.setWifiStatus(wifi);
    _test.setStarlink(starlink);
    _setup.update(wifi);
}

void UiManager::goToPage(int page, lv_screen_load_anim_t anim) {
    page = (page + kPageCount) % kPageCount;
    // Ignore while a slide is running: its outgoing screen is queued for
    // deletion and must not become the target again.
    if (page == _page || animating() || _hardwareTest) return;
    _page = page;
    lv_screen_load_anim(buildScreen(View::Page, page), anim, kSlideMs, 0, true);
    update();
}

void UiManager::showPage(int index) {
    _hardwareTest = false;
    goToPage(index, LV_SCREEN_LOAD_ANIM_NONE);
}

// LVGL event handlers run while the screen that raised them is alive;
// switching screens (which deletes the old one) is deferred until after.
void UiManager::requestUpdate() {
    if (_updatePending) return;
    _updatePending = true;
    lv_async_call(onDeferredUpdate, this);
}

void UiManager::onDeferredUpdate(void* ctx) {
    auto* self = static_cast<UiManager*>(ctx);
    self->_updatePending = false;
    self->update();
}

void UiManager::onGesture(lv_event_t* e) {
    auto* self = static_cast<UiManager*>(lv_event_get_user_data(e));
    const lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    // Finger moves left => next page slides in from the right. A slide
    // animation (not an instant load) never deletes the current screen
    // synchronously, so this is safe inside the event.
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
    self->requestUpdate();
}

void UiManager::onCloseHardwareTest(void* ctx) {
    auto* self = static_cast<UiManager*>(ctx);
    self->_hardwareTest = false;
    self->requestUpdate();
}

}  // namespace ui
