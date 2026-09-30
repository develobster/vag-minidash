#include "ui_platform.h"
bool md_target_has(const md_target_profile_t *target, uint32_t capabilities)
{
    return target && (target->hardware & capabilities) == capabilities;
}
bool md_target_feature_available(const md_target_profile_t *target, md_feature_t feature)
{
    if (!target || feature < 0 || feature >= MD_FEATURE_COUNT ||
        !(target->implemented_features & (1U << feature))) return false;
    if (feature == MD_FEATURE_WEB_CONFIG) return md_target_has(target, MD_CAP_WIFI);
    if (feature == MD_FEATURE_UDS || feature == MD_FEATURE_DTC_READ ||
        feature == MD_FEATURE_DTC_CLEAR) return md_target_has(target, MD_CAP_CAN);
    if (feature == MD_FEATURE_LOGGING) return md_target_has(target, MD_CAP_LOG_STORAGE);
    return true;
}
md_target_profile_t md_target_lite_proposal(void)
{
    return (md_target_profile_t){.name = "stm32-lite-proposal",
        .hardware = MD_CAP_MONOCHROME | MD_CAP_BUTTONS,
        .implemented_features = 0, .visible_widgets = 3, .verified = false};
}
