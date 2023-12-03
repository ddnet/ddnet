#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>
#import <Cocoa/Cocoa.h>

// TODO: NSUserNotification is deprecated. Use the User Notifications framework instead: https://developer.apple.com/documentation/usernotifications?language=objc
void NotificationsNotifyMacOsInternal(const char *pTitle, const char *pMessage)
{
	NSString* pNsTitle = [NSString stringWithCString:pTitle encoding:NSUTF8StringEncoding];
	NSString* pNsMsg = [NSString stringWithCString:pMessage encoding:NSUTF8StringEncoding];

	NSUserNotification *pNotification = [[NSUserNotification alloc] autorelease];
	pNotification.title = pNsTitle;
	pNotification.informativeText = pNsMsg;
	pNotification.soundName = NSUserNotificationDefaultSoundName;

	[[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:pNotification];

	[NSApp requestUserAttention:NSInformationalRequest]; // use NSCriticalRequest to annoy the user (doesn't stop bouncing)
}
