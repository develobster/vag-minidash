#pragma once

#include "ui_core.h"

const char *md_ui_i18n_text(md_ui_language_t language, const char *english);
const char *md_ui_i18n_signal_label(md_ui_language_t language,
                                    md_ui_signal_id_t signal);
const char *md_ui_i18n_compact_signal_label(md_ui_language_t language,
                                            md_ui_signal_id_t signal,
                                            const char *fallback);
