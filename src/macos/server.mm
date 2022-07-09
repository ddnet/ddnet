#import <Cocoa/Cocoa.h>

@interface ServerView : NSTextView
{
	NSTask *pTask;
	NSFileHandle *pFile;
}
- (void)listenTo: (NSTask *)t;
@end

@implementation ServerView
- (void)listenTo: (NSTask *)t
{
	NSPipe *pPipe;
	pTask = t;
	pPipe = [NSPipe pipe];
	[pTask setStandardOutput: pPipe];
	pFile = [pPipe fileHandleForReading];

	[[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(outputNotification:) name: NSFileHandleReadCompletionNotification object: pFile];

	[pFile readInBackgroundAndNotify];
}

- (void) outputNotification: (NSNotification *) notification
{
	NSData *pData = [[[notification userInfo] objectForKey: NSFileHandleNotificationDataItem] retain];
	NSString *pString = [[NSString alloc] initWithData: pData encoding: NSASCIIStringEncoding];
	NSAttributedString *pAttrStr = [[NSAttributedString alloc] initWithString: pString];

	[[self textStorage] appendAttributedString: pAttrStr];
	int length = [[self textStorage] length];
	NSRange range = NSMakeRange(length, 0);
	[self scrollRangeToVisible: range];

	[pAttrStr release];
	[pString release];
	[pFile readInBackgroundAndNotify];
}

-(void)windowWillClose:(NSNotification *)notification
{
	[pTask terminate];
	[NSApp terminate:self];
}
@end

void runServer()
{
	NSAutoreleasePool *pPool = [[NSAutoreleasePool alloc] init];
	NSApp = [NSApplication sharedApplication];
	NSBundle *pMainBundle = [NSBundle mainBundle];
	NSTask *pTask;
	pTask = [[NSTask alloc] init];
	[pTask setCurrentDirectoryPath: [pMainBundle resourcePath]]; // NOLINT(clang-analyzer-nullability.NullablePassedToNonnull)

	NSArray *pArguments = [NSArray new];

	NSAlert *pAlert = [[[NSAlert alloc] init] autorelease];
	[pAlert setMessageText: @"Run DDNet Server"];
	[pAlert addButtonWithTitle: @"Use default config"];
	[pAlert addButtonWithTitle: @"Select config"];
	[pAlert addButtonWithTitle: @"Cancel"];
	switch([pAlert runModal])
	{
		case NSAlertFirstButtonReturn:
			break;
		case NSAlertThirdButtonReturn:
			return;
		case NSAlertSecondButtonReturn:
			// get a server config
			NSOpenPanel *pOpenDlg = [NSOpenPanel openPanel];
			[pOpenDlg setCanChooseFiles:YES];
			if([pOpenDlg runModal] != NSModalResponseOK)
				return;
			NSString *pFileName = [[pOpenDlg URL] path];
			pArguments = [NSArray arrayWithObjects: @"-f", pFileName, nil];
			break;
	}

	// run server
	NSWindow *pWindow;
	ServerView *pView;
	NSRect GraphicsRect;

	GraphicsRect = NSMakeRect(100.0, 1000.0, 600.0, 400.0);

	pWindow = [[NSWindow alloc]
		initWithContentRect: GraphicsRect
		styleMask: NSWindowStyleMaskTitled
		| NSWindowStyleMaskClosable
		| NSWindowStyleMaskMiniaturizable
		backing: NSBackingStoreBuffered
		defer: NO];

	[pWindow setTitle: @"DDNet Server"];

	pView = [[[ServerView alloc] initWithFrame: GraphicsRect] autorelease];
	[pView setEditable: NO];
	[pView setRulerVisible: YES];

	[pWindow setContentView: pView];
	[pWindow setDelegate: (id<NSWindowDelegate>)pView];
	[pWindow makeKeyAndOrderFront: nil];

	[pView listenTo: pTask];
	[pTask setLaunchPath: [pMainBundle pathForAuxiliaryExecutable: @"DDNet-Server"]];
	[pTask setArguments: pArguments];
	[pTask launch];
	[NSApp run];
	[pTask terminate];

	[NSApp release];
	[pPool release];
}

int main(int argc, char **argv)
{
	runServer();

	return 0;
}
