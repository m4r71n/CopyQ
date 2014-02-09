/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "macplatform.h"

#include "common/common.h"
#include "copyqpasteboardmime.h"
#include "foregroundbackgroundfilter.h"
#include "macplatformwindow.h"
#include "platform/mac/macactivity.h"
#include "urlpasteboardmime.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScopedPointer>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace {
    class ClipboardApplication : public QApplication
    {
    public:
        static QApplication *create(int &argc, char **argv)
        {
            // Only try to create pasteboardMimes once, and only try if we have a native interface,
            // otherwise everything breaks when QPasteboardMime tries to resove some native functions.
            if (QGuiApplication::platformNativeInterface())
                return new ClipboardApplication(argc, argv);

            return new QApplication(argc, argv);
        }

    private:
        ClipboardApplication(int &argc, char **argv)
            : QApplication(argc, argv)
            , m_pasteboardMime()
            , m_pasteboardMimeUrl(QLatin1String("public.url"))
            , m_pasteboardMimeFileUrl(QLatin1String("public.file-url"))
        {
        }

        CopyQPasteboardMime m_pasteboardMime;
        UrlPasteboardMime m_pasteboardMimeUrl;
        UrlPasteboardMime m_pasteboardMimeFileUrl;
    };

    template<typename T> inline T* objc_cast(id from)
    {
        if ([from isKindOfClass:[T class]]) {
            return static_cast<T*>(from);
        }
        return nil;
    }

    bool isApplicationInItemList(LSSharedFileListRef list) {
        bool flag = false;
        UInt32 seed;
        CFArrayRef items = LSSharedFileListCopySnapshot(list, &seed);
        if (items) {
            CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
            if (url) {
                for (id item in(__bridge NSArray *) items) {
                    LSSharedFileListItemRef itemRef = (__bridge LSSharedFileListItemRef)item;
                    if (LSSharedFileListItemResolve(itemRef, 0, &url, NULL) == noErr) {
                        if ([[(__bridge NSURL *) url path] hasPrefix:[[NSBundle mainBundle] bundlePath]]) {
                            flag = true;
                            break;
                        }
                    }
                }
            }
            CFRelease(items);
        }
        return flag;
    }

    void addToLoginItems()
    {
        LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
        if (list) {
            if (!isApplicationInItemList(list)) {
                CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
                if (url) {
                    // Don't "Hide on Launch", as we don't have a window to show anyway
                    NSDictionary *properties = [NSDictionary
                        dictionaryWithObject: [NSNumber numberWithBool:NO]
                        forKey: @"com.apple.loginitem.HideOnLaunch"];
                    LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, NULL, NULL, url, (__bridge CFDictionaryRef)properties, NULL);
                    if (item)
                        CFRelease(item);
                } else {
                    ::log("Unable to find url for bundle, can't auto-load app", LogWarning);
                }
            }
            CFRelease(list);
        } else {
            ::log("Unable to access shared file list, can't auto-load app", LogWarning);
        }
    }

    void removeFromLoginItems()
    {
        LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
        if (list) {
            if (isApplicationInItemList(list)) {
                CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
                if (url) {
                    UInt32 seed;
                    CFArrayRef items = LSSharedFileListCopySnapshot(list, &seed);
                    if (items) {
                        for (id item in(__bridge NSArray *) items) {
                            LSSharedFileListItemRef itemRef = (__bridge LSSharedFileListItemRef)item;
                            if (LSSharedFileListItemResolve(itemRef, 0, &url, NULL) == noErr)
                                if ([[(__bridge NSURL *) url path] hasPrefix:[[NSBundle mainBundle] bundlePath]])
                                    LSSharedFileListItemRemove(list, itemRef);
                        }
                        CFRelease(items);
                    } else {
                        ::log("No items in list of auto-loaded apps, can't stop auto-load of app", LogWarning);
                    }
                } else {
                    ::log("Unable to find url for bundle, can't stop auto-load of app", LogWarning);
                }
            }
            CFRelease(list);
        } else {
            ::log("Unable to access shared file list, can't stop auto-load of app", LogWarning);
        }
    }
} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new MacPlatform);
}

MacPlatform::MacPlatform()
{
}

QApplication *MacPlatform::createServerApplication(int &argc, char **argv)
{
    MacActivity activity(MacActivity::Background, "CopyQ Server");
    QApplication *app = ClipboardApplication::create(argc, argv);
    ForegroundBackgroundFilter::installFilter(app);
    return app;
}

QApplication *MacPlatform::createMonitorApplication(int &argc, char **argv)
{
    MacActivity activity(MacActivity::Background, "CopyQ clipboard monitor");
    return ClipboardApplication::create(argc, argv);
}

QCoreApplication *MacPlatform::createClientApplication(int &argc, char **argv)
{
    MacActivity activity(MacActivity::User, "CopyQ Client");
    return new QCoreApplication(argc, argv);
}

PlatformWindowPtr MacPlatform::getCurrentWindow()
{
    NSRunningApplication *runningApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    return PlatformWindowPtr(new MacPlatformWindow(runningApp));
}

PlatformWindowPtr MacPlatform::getWindow(WId winId) {
    return PlatformWindowPtr(new MacPlatformWindow(winId));
}

long int MacPlatform::getChangeCount()
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger changeCount = [pasteboard changeCount];
    return changeCount;
}

bool MacPlatform::isAutostartEnabled()
{
    // Note that this will need to be done differently if CopyQ goes into
    // the App Store.
    // http://rhult.github.io/articles/sandboxed-launch-on-login/
    bool isInList = false;
    LSSharedFileListRef list = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL);
    if (list) {
        isInList = isApplicationInItemList(list);
        CFRelease(list);
    }
    return isInList;
}

void MacPlatform::setAutostartEnabled(bool shouldEnable)
{
    if (shouldEnable != isAutostartEnabled()) {
        if (shouldEnable) {
            addToLoginItems();
        } else {
            removeFromLoginItems();
        }
    }
}
