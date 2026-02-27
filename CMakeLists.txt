cmake_minimum_required(VERSION 4.0)
project(kuni)

set(TELEGRAM_API_TOKEN "" CACHE STRING "Telegram BOT API token (bot_id:secret). Mandatory")
set(OPENAICHAT_ADDRESS "localhost:11434" CACHE STRING "Address to privateGPT")

if (NOT TELEGRAM_API_TOKEN)
    message(FATAL_ERROR "Please set TELEGRAM_API_TOKEN.")
endif()

set(AUI_VERSION 6d0908466c44959edffba506e850948f9c50d087)
option(BUILD_SHARED_LIBS OFF)

# Use AUI.Boot
file(
        DOWNLOAD
        https://raw.githubusercontent.com/aui-framework/aui/${AUI_VERSION}/aui.boot.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/aui.boot.cmake)
include(${CMAKE_CURRENT_BINARY_DIR}/aui.boot.cmake)

# import AUI
auib_import(aui https://github.com/aui-framework/aui
        COMPONENTS core json curl crypt image
        VERSION ${AUI_VERSION})


auib_import(date https://github.com/HowardHinnant/date)

aui_executable(${PROJECT_NAME})
aui_enable_tests(${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE TELEGRAM_API_TOKEN=${TELEGRAM_API_TOKEN} OPENAICHAT_ADDRESS=${OPENAICHAT_ADDRESS})
target_compile_definitions(Tests PRIVATE TELEGRAM_API_TOKEN=${TELEGRAM_API_TOKEN} OPENAICHAT_ADDRESS=${OPENAICHAT_ADDRESS})

# Link required libs
aui_link(${PROJECT_NAME} PUBLIC aui::core aui::json aui::curl aui::crypt aui::image)
