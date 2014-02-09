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

#include "macplatformwindow.h"

#include <common/common.h>

#include <AppKit/NSGraphics.h>
#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <dispatch/dispatch.h>

#include <QApplication>
#include <QSet>

namespace {
    template<typename T> inline T* objc_cast(id from)
    {
        if ([from isKindOfClass:[T class]]) {
            return static_cast<T*>(from);
        }
        return nil;
    }

    void sendCommandV() {
        CGEventSourceRef sourceRef = CGEventSourceCreate(
            kCGEventSourceStateCombinedSessionState);

        CGEventRef commandDown = CGEventCreateKeyboardEvent(sourceRef, kVK_Command, YES);
        CGEventRef VDown = CGEventCreateKeyboardEvent(sourceRef, kVK_ANSI_V, YES);

        CGEventRef VUp = CGEventCreateKeyboardEvent(sourceRef, kVK_ANSI_V, NO);
        CGEventRef commandUp = CGEventCreateKeyboardEvent(sourceRef, kVK_Command, NO);

        // 0x000008 is a hack to fix pasting in Emacs?
        // https://github.com/TermiT/Flycut/pull/18
        CGEventSetFlags(VDown,kCGEventFlagMaskCommand|0x000008);
        CGEventSetFlags(VUp,kCGEventFlagMaskCommand|0x000008);

        CGEventPost(kCGHIDEventTap, commandDown);
        CGEventPost(kCGHIDEventTap, VDown);
        CGEventPost(kCGHIDEventTap, VUp);
        CGEventPost(kCGHIDEventTap, commandUp);

        CFRelease(commandDown);
        CFRelease(VDown);
        CFRelease(VUp);
        CFRelease(commandUp);
        CFRelease(sourceRef);
    }

    /**
     * Delays a paste operation for delayInMS, and retries up to 'tries' times.
     *
     * This function is necessary in order to check that the intended window has come
     * to the foreground. The "isActive" property only changes when the app gets back
     * to the "run loop", so we can't block and check it.
     */
    void delayedPaste(int64_t delayInMS, uint tries, NSWindow *window, NSRunningApplication *app) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delayInMS * NSEC_PER_MSEC), dispatch_get_main_queue(), ^(void){
            if (![app isActive] || (window && ![window isKeyWindow])) {
                if (tries > 0) {
                    delayedPaste(delayInMS, tries - 1, window, app);
                } else {
                    log("Failed to raise application, will not paste.", LogWarning);
                }
            } else {
                // Send Command-V
                sendCommandV();
            }
        });
    }

    long int getTopWindow(pid_t process_pid) {
        // Build a set of "normal" windows. This is necessary as "NSWindowList" gets things like the
        // menubar (which can be "owned" by various apps).
        QSet<long int> widsForProcess;
        NSArray *array = (__bridge NSArray*) CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        for (NSDictionary* dict in array) {
            long int pid = [(NSNumber*)[dict objectForKey:@"kCGWindowOwnerPID"] longValue];
            long int wid = [(NSNumber*)[dict objectForKey:@"kCGWindowNumber"] longValue];
            long int layer = [(NSNumber*)[dict objectForKey:@"kCGWindowLayer"] longValue];

            if (pid == process_pid && layer == 0) {
                widsForProcess.insert(wid);
            }
        }

        // Now look through the windows in NSWindowList (which are ordered from front to back)
        // the first window in this list which is also in widsForProcess is our frontmost "normal" window
        long int wid = -1;
        NSInteger windowCount, *windows;
        NSCountWindows(&windowCount);
        windows = (NSInteger*) malloc(windowCount * sizeof(NSInteger));
        if (windows) {
            NSWindowList(windowCount, windows);
            for (int i = 0; i < windowCount; ++i) {
                if (widsForProcess.contains(windows[i])) {
                    wid = windows[i];
                    break;
                }
            }
            free(windows);
        }

        return wid;
    }

    QString getTitleFromWid(long int wid) {
        if (wid < 0) {
            return QString();
        }

        uint32_t windowid[1] = {wid};
        CFArrayRef windowArray = CFArrayCreate ( NULL, (const void **)windowid, 1 ,NULL);
        NSArray *array = (__bridge NSArray*) CGWindowListCreateDescriptionFromArray(windowArray);

        QString title;
        for (NSDictionary* dict in array) {
            title = QString::fromNSString([dict objectForKey:@"kCGWindowName"]);
        }
        CFRelease(windowArray);

        return title;
    }
} // namespace

MacPlatformWindow::MacPlatformWindow(NSRunningApplication *runningApp):
    m_windowNumber(-1)
    , m_window(0)
{
    if (runningApp) {
        m_runningApplication = runningApp;
        [runningApp retain];
        m_windowNumber = getTopWindow(runningApp.processIdentifier);
        COPYQ_LOG("Created platform window for non-copyq");
    } else {
        log("Failed to convert runningApplication to application", LogWarning);
    }
}

MacPlatformWindow::MacPlatformWindow(WId wid):
    m_windowNumber(-1)
{
    NSView *view = objc_cast<NSView>((id)wid);
    if (view) {
        // If given a view, its ours
        m_runningApplication = [NSRunningApplication currentApplication];
        m_window = [view window];
        [m_runningApplication retain];
        [m_window retain];
        COPYQ_LOG("Created platform window for copyq");
    } else {
        log("Failed to convert WId to window", LogWarning);
    }
}

MacPlatformWindow::MacPlatformWindow():
    m_windowNumber(-1)
    , m_window(0)
    , m_runningApplication(0)
{
}

MacPlatformWindow::~MacPlatformWindow() {
    // Releasing '0' or 'nil' is fine
    [m_runningApplication release];
    [m_window release];
}

QString MacPlatformWindow::getTitle()
{
    QString appTitle;
    if (m_runningApplication)
        appTitle = QString::fromNSString([m_runningApplication localizedName]);

    QString windowTitle;
    if (m_window)
        windowTitle = QString::fromNSString([m_window title]);
    if (windowTitle.isEmpty() && m_windowNumber >= 0)
        windowTitle = getTitleFromWid(m_windowNumber);

    // We have two separate titles, the application title (shown at the top
    // left in the menu bar and the window title (shown in the window bar).
    // Some apps put the app name into the window title as well, some don't. We
    // want to be able to match on the app name (e.g. Firefox, Safari, or
    // Terminal) when writing commands, so we'll ensure that the app name is at
    // the beginning of the returned window title.
    QString result;
    if (windowTitle.isEmpty()) {
        result = appTitle;
    } else if (appTitle.isEmpty() || windowTitle.startsWith(appTitle)) {
        result = windowTitle;
    } else {
        result = QString("%1 - %2").arg(appTitle, windowTitle);
    }
    return result;
}

void MacPlatformWindow::raise()
{
    if (m_window && m_runningApplication &&
            [m_runningApplication isEqual:[NSRunningApplication currentApplication]]) {
        COPYQ_LOG(QString("Raise CopyQ"));
        [NSApp activateIgnoringOtherApps:YES];
        [m_window makeKeyAndOrderFront:nil];
    } else if (m_runningApplication) {
        // Shouldn't need to unhide since we should have just been in this
        // application..
        //[m_runningApplication unhide];

        COPYQ_LOG(QString("Raise running app."));
        [m_runningApplication activateWithOptions:0];
    } else {
        ::log(QString("Tried to raise unknown window."), LogWarning);
    }
}

void MacPlatformWindow::pasteClipboard()
{
    if (!m_runningApplication) {
        log("Failed to paste to unknown window", LogWarning);
        return;
    }

    // Window MUST be raised, otherwise we can't send events to it
    raise();

    // Paste after after 100ms, try 5 times
    delayedPaste(100, 5, m_window, m_runningApplication);
}
