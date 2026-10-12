# This file is part of Soulgram,
# an unofficial fork of Telegram Desktop.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

# Soulgram sources live in their own folder and are listed here,
# so that merging upstream changes to CMakeLists.txt stays trivial.
nice_target_sources(Telegram ${src_loc}
PRIVATE
    soulgram/soulgram_api_keys.cpp
    soulgram/soulgram_api_keys.h
    soulgram/soulgram_chat_actions.cpp
    soulgram/soulgram_chat_actions.h
    soulgram/soulgram_extras.cpp
    soulgram/soulgram_extras.h
    soulgram/soulgram_filters.cpp
    soulgram/soulgram_filters.h
    soulgram/soulgram_forward.cpp
    soulgram/soulgram_forward.h
    soulgram/soulgram_lang.cpp
    soulgram/soulgram_lang.h
    soulgram/soulgram_links.cpp
    soulgram/soulgram_links.h
    soulgram/soulgram_messages.cpp
    soulgram/soulgram_messages.h
    soulgram/soulgram_premium.cpp
    soulgram/soulgram_premium.h
    soulgram/soulgram_regdate.cpp
    soulgram/soulgram_regdate.h
    soulgram/soulgram_settings.cpp
    soulgram/soulgram_settings.h
    soulgram/soulgram_settings_section.cpp
    soulgram/soulgram_settings_section.h
    soulgram/soulgram_shot.cpp
    soulgram/soulgram_shot.h
    soulgram/soulgram_storage.cpp
    soulgram/soulgram_storage.h
    soulgram/soulgram_stories.cpp
    soulgram/soulgram_stories.h
    soulgram/soulgram_ui.cpp
    soulgram/soulgram_ui.h
    soulgram/soulgram_updates.cpp
    soulgram/soulgram_updates.h
    soulgram/soulgram_view_once.cpp
    soulgram/soulgram_view_once.h
)

# api_id / api_hash go only into this file, so changing them
# rebuilds one file instead of the whole application.
set_source_files_properties(${src_loc}/soulgram/soulgram_api_keys.cpp
PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_DEFINITIONS "TDESKTOP_API_ID=${TDESKTOP_API_ID};TDESKTOP_API_HASH=${TDESKTOP_API_HASH}"
)
