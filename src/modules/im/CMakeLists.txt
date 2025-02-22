project(IM)
add_definitions(-DOK_${PROJECT_NAME}_MODULE="${PROJECT_NAME}")

include_directories(${PROJECT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})

pkg_check_modules(QRENCODE REQUIRED libqrencode)
pkg_check_modules(exif REQUIRED libexif)
pkg_check_modules(JPEG REQUIRED libjpeg)

include_directories(${QRENCODE_INCLUDE_DIRS})
include_directories(${exif_INCLUDE_DIRS})
include_directories(${JPEG_INCLUDE_DIRS})

list(APPEND ALL_LIBRARIES
		${QRENCODE_LINK_LIBRARIES}
		${JPEG_LINK_LIBRARIES}
		${exif_LINK_LIBRARIES}
)

set(CMAKE_AUTOUIC_SEARCH_PATHS ./src src/widget src/widget/form)

qt5_wrap_ui(
		${PROJECT_NAME}_FORMS
		src/mainwindow.ui
		src/chatlog/content/filetransferwidget.ui
		src/widget/ChatWidget.ui
		src/widget/ContactWidget.ui
		src/widget/form/aboutfriendform.ui
		src/widget/form/aboutgroupform.ui
		src/widget/form/addfriendform.ui
		src/widget/form/loadhistorydialog.ui
		src/widget/form/profileform.ui
		src/widget/tool/removefrienddialog.ui
		src/widget/form/searchsettingsform.ui
		src/widget/form/setpassworddialog.ui
		src/widget/form/settings/avform.ui
		src/widget/form/settings/generalsettings.ui
		src/widget/form/settings/StorageSettingsForm.ui
		src/widget/form/CallDurationForm.ui
)

set(${PROJECT_NAME}_SOURCES
		src/chatlog/chatlinecontent.cpp
		src/chatlog/chatlinecontent.h
		src/chatlog/chatlinecontentproxy.cpp
		src/chatlog/chatlinecontentproxy.h
		src/chatlog/chatline.cpp
		src/chatlog/chatline.h
		src/chatlog/chatmessageitem.cpp
		src/chatlog/chatmessageitem.h
		src/chatlog/chatlog.cpp
		src/chatlog/chatlog.h
		src/chatlog/chatmessage.cpp
		src/chatlog/chatmessage.h
		src/chatlog/content/filetransferwidget.cpp
		src/chatlog/content/filetransferwidget.h
		src/chatlog/content/image.cpp
		src/chatlog/content/image.h
		src/chatlog/content/notificationicon.cpp
		src/chatlog/content/notificationicon.h
		src/chatlog/content/spinner.cpp
		src/chatlog/content/spinner.h
		src/chatlog/content/text.cpp
		src/chatlog/content/text.h
		src/chatlog/content/timestamp.cpp
		src/chatlog/content/timestamp.h
		src/chatlog/content/broken.cpp
		src/chatlog/content/broken.h
		src/chatlog/content/contactavatar.cpp
		src/chatlog/content/contactavatar.h
		src/chatlog/content/simpletext.cpp
		src/chatlog/content/simpletext.h
		src/chatlog/customtextdocument.cpp
		src/chatlog/customtextdocument.h
		src/chatlog/documentcache.cpp
		src/chatlog/documentcache.h
		src/chatlog/pixmapcache.cpp
		src/chatlog/pixmapcache.h
		src/chatlog/toxfileprogress.cpp
		src/chatlog/toxfileprogress.h
		src/chatlog/textformatter.cpp
		src/chatlog/textformatter.h
		src/core/coreav.cpp
		src/core/coreav.h
		src/core/core.h
		src/core/core.cpp
		src/core/corefile.cpp
		src/core/corefile.h
		src/core/icoresettings.h
		src/core/toxcall.cpp
		src/core/toxcall.h
		src/core/toxencrypt.cpp
		src/core/toxencrypt.h
		src/core/toxfile.cpp
		src/core/toxfile.h
		src/core/toxfilepause.h
		src/core/toxid.cpp
		src/core/toxid.h
		src/core/toxoptions.cpp
		src/core/toxoptions.h
		src/model/groupid.h
		src/model/groupid.cpp
		src/model/contactid.h
		src/model/contactid.cpp
		src/model/FriendId.h
		src/model/FriendId.cpp
		src/model/friendlist.cpp
		src/model/friendlist.h
		src/model/grouplist.cpp
		src/model/grouplist.h
		src/model/aboutfriend.cpp
		src/model/aboutfriend.h
		src/model/iaboutfriend.h
		src/model/chatroom/chatroom.h
		src/model/chatroom/chatroom.cpp
		src/model/chatroom/friendchatroom.cpp
		src/model/chatroom/friendchatroom.h
		src/model/chatroom/groupchatroom.cpp
		src/model/chatroom/groupchatroom.h
		src/model/contact.cpp
		src/model/contact.h
		src/model/chatlogitem.cpp
		src/model/chatlogitem.h
		src/model/friend.cpp
		src/model/friend.h
		src/model/message.h
		src/model/message.cpp
		src/model/imessagedispatcher.h
		src/model/friendmessagedispatcher.h
		src/model/friendmessagedispatcher.cpp
		src/model/groupmessagedispatcher.h
		src/model/groupmessagedispatcher.cpp
		src/model/message.h
		src/model/message.cpp
		src/model/groupinvite.cpp
		src/model/groupinvite.h
		src/model/group.cpp
		src/model/group.h
		src/model/status.cpp
		src/model/status.h
		src/model/profile/iprofileinfo.h
		src/model/profile/profileinfo.cpp
		src/model/profile/profileinfo.h
		src/model/dialogs/idialogs.h
		src/model/ichatlog.h
		src/model/sessionchatlog.h
		src/model/sessionchatlog.cpp
		src/model/chathistory.h
		src/model/chathistory.cpp
		src/model/toxclientstandards.h
		src/nexus.cpp
		src/nexus.h

		src/persistence/history.cpp
		src/persistence/history.h
		src/persistence/ifriendsettings.h
		src/persistence/offlinemsgengine.cpp
		src/persistence/offlinemsgengine.h
		src/persistence/paths.cpp
		src/persistence/paths.h
		src/persistence/profile.h
		src/persistence/profile.cpp
		src/persistence/profilelocker.cpp
		src/persistence/profilelocker.h
		src/persistence/serialize.cpp
		src/persistence/serialize.h
		src/persistence/settings.cpp
		src/persistence/settings.h
		src/persistence/settingsserializer.cpp
		src/persistence/settingsserializer.h
		src/persistence/smileypack.cpp
		src/persistence/smileypack.h
		src/video/videosurface.cpp
		src/video/videosurface.h
		src/video/genericnetcamview.h
		src/video/genericnetcamview.cpp
		src/video/netcamview.h
		src/video/netcamview.cpp
		src/video/corevideosource.h
		src/video/corevideosource.cpp
		src/widget/form/aboutfriendform.cpp
		src/widget/form/aboutfriendform.h
		src/widget/chatformheader.cpp
		src/widget/chatformheader.h
		src/widget/contentdialog.cpp
		src/widget/contentdialog.h
		src/widget/contentdialogmanager.cpp
		src/widget/contentdialogmanager.h
		src/widget/contentlayout.cpp
		src/widget/contentlayout.h
		src/widget/emoticonswidget.cpp
		src/widget/emoticonswidget.h
		src/widget/flowlayout.cpp
		src/widget/flowlayout.h
		#src/widget/searchform.cpp
		#src/widget/searchform.h
		src/widget/searchtypes.h
		src/widget/ContactWidget.h
		src/widget/ContactWidget.cpp
		src/widget/MainLayout.h
		src/widget/MainLayout.cpp
		src/widget/MessageSessionListWidget.h
		src/widget/MessageSessionListWidget.cpp
		src/widget/MessageSessionWidget.h
		src/widget/MessageSessionWidget.cpp
		src/widget/form/aboutgroupform.h
		src/widget/form/aboutgroupform.cpp
		src/widget/form/addfriendform.cpp
		src/widget/form/addfriendform.h
		src/widget/form/friendform.h
		src/widget/form/friendform.cpp
		src/widget/form/friendform.ui
		src/widget/form/chatform.cpp
		src/widget/form/chatform.h
		src/widget/form/ChatInputForm.h
		src/widget/form/ChatInputForm.cpp
		src/widget/form/ChatReplyForm.ui
		src/widget/form/ChatReplyForm.h
		src/widget/form/ChatReplyForm.cpp
		src/widget/form/ContactSelectDialog.h
		src/widget/form/ContactSelectDialog.cpp
		src/widget/form/ContactSelectDialog.ui
		src/widget/form/filesform.cpp
		src/widget/form/filesform.h
		src/widget/form/genericchatform.cpp
		src/widget/form/genericchatform.h
		src/widget/form/groupchatform.cpp
		src/widget/form/groupchatform.h
		src/widget/form/groupinviteform.cpp
		src/widget/form/groupinviteform.h
		src/widget/form/groupinvitewidget.cpp
		src/widget/form/groupinvitewidget.h
		src/widget/form/GroupCreateForm.h
		src/widget/form/GroupCreateForm.cpp
		src/widget/form/GroupCreateForm.ui
		src/widget/form/loadhistorydialog.cpp
		src/widget/form/loadhistorydialog.h
		src/widget/form/profileform.cpp
		src/widget/form/profileform.h
		src/widget/form/searchsettingsform.cpp
		src/widget/form/searchsettingsform.h
		src/widget/form/setpassworddialog.cpp
		src/widget/form/setpassworddialog.h
		src/widget/form/settings/avform.cpp
		src/widget/form/settings/avform.h
		src/widget/form/settings/generalform.cpp
		src/widget/form/settings/generalform.h
		src/widget/form/settings/genericsettings.cpp
		src/widget/form/settings/genericsettings.h
		src/widget/form/settings/StorageSettingsForm.h
		src/widget/form/settings/StorageSettingsForm.cpp
		src/widget/form/settingswidget.cpp
		src/widget/form/settingswidget.h
		src/widget/form/tabcompleter.cpp
		src/widget/form/tabcompleter.h
		src/widget/form/CallDurationForm.h
		src/widget/form/CallDurationForm.cpp
		src/widget/ContactListLayout.cpp
		src/widget/ContactListLayout.h
		src/widget/ContactListWidget.cpp
		src/widget/ContactListWidget.h

		src/widget/friendwidget.cpp
		src/widget/friendwidget.h
		src/widget/genericchatitemlayout.cpp
		src/widget/genericchatitemlayout.h
		src/widget/genericchatitemwidget.cpp
		src/widget/genericchatitemwidget.h
		src/widget/genericchatroomwidget.cpp
		src/widget/genericchatroomwidget.h
		src/widget/groupwidget.cpp
		src/widget/groupwidget.h
		src/widget/maskablepixmapwidget.cpp
		src/widget/maskablepixmapwidget.h
		src/widget/notificationedgewidget.cpp
		src/widget/notificationedgewidget.h
		src/widget/notificationscrollarea.cpp
		src/widget/notificationscrollarea.h
		src/widget/passwordedit.cpp
		src/widget/passwordedit.h
		src/widget/qrwidget.cpp
		src/widget/qrwidget.h
		src/widget/splitterrestorer.cpp
		src/widget/splitterrestorer.h
		src/widget/tool/activatedialog.cpp
		src/widget/tool/activatedialog.h
		src/widget/tool/adjustingscrollarea.cpp
		src/widget/tool/adjustingscrollarea.h
		src/widget/tool/callconfirmwidget.cpp
		src/widget/tool/callconfirmwidget.h
		src/widget/tool/chattextedit.cpp
		src/widget/tool/chattextedit.h
		src/widget/tool/croppinglabel.cpp
		src/widget/tool/croppinglabel.h
		src/widget/tool/flyoutoverlaywidget.cpp
		src/widget/tool/flyoutoverlaywidget.h
		src/widget/tool/friendrequestdialog.cpp
		src/widget/tool/friendrequestdialog.h
		src/widget/tool/movablewidget.cpp
		src/widget/tool/movablewidget.h
		src/widget/tool/profileimporter.cpp
		src/widget/tool/profileimporter.h
		src/widget/tool/removefrienddialog.cpp
		src/widget/tool/removefrienddialog.h
		src/widget/tool/screengrabberchooserrectitem.cpp
		src/widget/tool/screengrabberchooserrectitem.h
		src/widget/tool/screengrabberoverlayitem.cpp
		src/widget/tool/screengrabberoverlayitem.h
		src/widget/tool/screenshotgrabber.cpp
		src/widget/tool/screenshotgrabber.h
		src/widget/tool/toolboxgraphicsitem.cpp
		src/widget/tool/toolboxgraphicsitem.h
		src/widget/ChatWidget.h
		src/widget/ChatWidget.cpp
		src/widget/ContentWidget.cpp
		src/widget/ContentWidget.h
		src/widget/widget.cpp
		src/widget/widget.h
		src/worker/SendWorker.h
		src/worker/SendWorker.cpp
)



if (UNIX)
	set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES}
			src/platform/posixsignalnotifier.cpp
			src/platform/posixsignalnotifier.h)
endif ()

if (PLATFORM_EXTENSIONS)
	set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES}
			src/platform/capslock.h
			src/platform/timer.h)
	if (WIN32)
		set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES}
				src/platform/capslock_win.cpp
				src/platform/timer_win.cpp)
	elseif (${X11_EXT})
		set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES}
				src/platform/capslock_x11.cpp
				src/platform/timer_x11.cpp
				src/platform/x11_display.cpp)
	elseif (${APPLE_EXT})
		set(${PROJECT_NAME}_SOURCES ${${PROJECT_NAME}_SOURCES}
				src/platform/capslock_osx.cpp
				src/platform/timer_osx.cpp
		)
	endif ()
endif ()

file(GLOB ${PROJECT_NAME}_TSS
		ts/*.xml
)

qt5_add_translation(
		${PROJECT_NAME}_QM_FILES
		${${PROJECT_NAME}_TSS}
)

set(RESOURCE_NAME "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.qrc.in")

file(WRITE "${RESOURCE_NAME}"
		"<!DOCTYPE RCC>
<RCC version=\"1.0\">
  <qresource prefix=\"/translations/${PROJECT_NAME}\">
")

foreach (qm ${${PROJECT_NAME}_QM_FILES})
	get_filename_component(qm_name ${qm} NAME)
	file(APPEND "${RESOURCE_NAME}"
			"    <file alias=\"${qm_name}\">${qm}</file>\n")
endforeach (qm)

file(APPEND "${RESOURCE_NAME}"
		"</qresource>
</RCC>
")

execute_process(
		COMMAND
		${CMAKE_COMMAND} -E copy_if_different
		${RESOURCE_NAME}
		${PROJECT_BINARY_DIR}/${PROJECT_NAME}.qrc)

set(SMILEY_RESOURCES
		${PROJECT_SOURCE_DIR}/smileys/emojione.qrc
		${PROJECT_SOURCE_DIR}/smileys/smileys.qrc)

qt5_add_resources(
		${PROJECT_NAME}_RESOURCES
		res.qrc
		${PROJECT_BINARY_DIR}/${PROJECT_NAME}.qrc
		${SMILEY_RESOURCES}
		# DEPENDS ${${PROJECT_NAME}_QM_FILES} # OPTIONS ${RCC_OPTIONS}
)

add_library(${PROJECT_NAME} STATIC
		${${PROJECT_NAME}_FORMS}
		${${PROJECT_NAME}_SOURCES}
		${${PROJECT_NAME}_QM_FILES}
		${${PROJECT_NAME}_RESOURCES}
)

if (WIN32)
	set(PLATFORM_LIBS   # 3D
			d3d11
			# GDI图形API
			gdi32
			# DirectX
			dxgi dxva2
			# Windows Media
			Winmm dmoGuids WMCodecDspUuid avrt
			# Media Foundation API
			Mf Mfplat Mfreadwrite mfuuid evr #Enhanced Video Renderer 视频渲染器
			# DirectShow
			amstrmid msdmo strmiids strmbase
			# COM
			Ole32 OleAut32 uuid OLDNAMES comsuppd
			# Windows DWM API
			dwmapi Shcore
			# Shell Light-Weight Utility API
			shlwapi
			# IP Helper API
			iphlpapi
			# 核心API
			kernel32 user32 Vfw32 shell32 advapi32
	)

	if (CMAKE_BUILD_TYPE MATCHES "Debug")
		set(PLATFORM_LIBS ${PLATFORM_LIBS} MSVCRTD)
	else ()
		set(PLATFORM_LIBS ${PLATFORM_LIBS} MSVCRT)
	endif ()

elseif (APPLE)

	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(CORE_FOUNDATION CoreFoundation)
	find_library(CORE_SERVICES CoreServices)
	find_library(COREGRAPHICS CoreGraphics)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_VIDEO CoreVIDEO)
	find_library(CoreMedia CoreMedia)
	find_library(AudioToolbox AudioToolbox)
	find_library(VideoToolbox VideoToolbox)
	find_library(OpenGL OpenGL)
	find_library(IOKit IOKit)
	find_library(AppKit AppKit)
	find_library(IOSurface IOSurface)
	find_library(AVFOUNDATION AVFoundation)
	find_library(Foundation Foundation)
	find_library(Metal Metal)

	set(PLATFORM_LIBS ${AppKit} ${Metal} ${IOSurface} ${AudioToolbox} ${VideoToolbox}
			${AVFOUNDATION} ${Foundation} ${IOKit} ${OpenGL}
			${CoreMedia} ${CORE_AUDIO} ${CORE_VIDEO} ${COREGRAPHICS}
			${CORE_FOUNDATION} ${APPLICATION_SERVICES} ${CORE_SERVICES})

elseif (LINUX)
	set(PLATFORM_LIBS z atomic X11)
endif ()

target_link_libraries(${PROJECT_NAME}
		PRIVATE OkIMCore
		PRIVATE OkStorage
		PRIVATE OkAudio
		PRIVATE OkVideo
		PRIVATE ${ok-gloox_LIBRARIES}
		PUBLIC ${Qt5Core_LIBRARIES}
		PUBLIC ${Qt5Gui_LIBRARIES}
		PUBLIC ${Qt5Widgets_LIBRARIES}
		PUBLIC ${Qt5Network_LIBRARIES}
		PUBLIC ${Qt5Xml_LIBRARIES}
		PUBLIC ${Qt5Svg_LIBRARIES}
		PUBLIC ${CMAKE_REQUIRED_LIBRARIES}
		PUBLIC ${ALL_LIBRARIES}
		PUBLIC ${PLATFORM_LIBS}
		PUBLIC range-v3::meta range-v3::concepts range-v3::range-v3
)

if (WIN32)
	set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

