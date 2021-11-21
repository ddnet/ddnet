#import <Cocoa/Cocoa.h>
#include <base/system.h>

extern int TWMain(int argc, char **argv);

int main(int argc, char **argv)
{
	BOOL FinderLaunch = argc >= 2 && !str_comp_num(argv[1], "-psn", 4);

	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	if(!resourcePath)
		return -1;

	[[NSFileManager defaultManager] changeCurrentDirectoryPath:resourcePath];

	if(FinderLaunch)
	{
		char *paArgv[2] = { argv[0], NULL };
		return TWMain(1, paArgv);
	}
	else
		return TWMain(argc, argv);
}
