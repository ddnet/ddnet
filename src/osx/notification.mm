#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>
#import <Cocoa/Cocoa.h>
#import "notification.h"

void CNotification::Notify(const char *pTitle, const char *pMsg)
{
	NSString* pNsTitle = [NSString stringWithCString:pTitle encoding:NSUTF8StringEncoding];
	NSString* pNsMsg = [NSString stringWithCString:pMsg encoding:NSUTF8StringEncoding];

	NSUserNotification *pNotification = [[NSUserNotification alloc] autorelease];
	pNotification.title = pNsTitle;
	pNotification.informativeText = pNsMsg;
	pNotification.soundName = NSUserNotificationDefaultSoundName;

	[[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:pNotification];

	[NSApp requestUserAttention:NSInformationalRequest]; // use NSCriticalRequest to annoy the user (doesn't stop bouncing)
}
