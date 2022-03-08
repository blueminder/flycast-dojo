//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#include <sys/stat.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#include "types.h"
#include "log/LogManager.h"
#include "rend/gui.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#endif
#include "stdclass.h"
#include "oslib/oslib.h"
#include "emulator.h"
#include "rend/mainui.h"

int darw_printf(const char* text, ...)
{
    va_list args;

    char temp[2048];
    va_start(args, text);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
}

void UpdateInputState() {
#if defined(USE_SDL)
	input_sdl_handle();
#endif
}

void os_CreateWindow() {
#ifdef DEBUG
    int ret = task_set_exception_ports(
                                       mach_task_self(),
                                       EXC_MASK_BAD_ACCESS,
                                       MACH_PORT_NULL,
                                       EXCEPTION_DEFAULT,
                                       0);
    
    if (ret != KERN_SUCCESS) {
        printf("task_set_exception_ports: %s\n", mach_error_string(ret));
    }
#endif
	sdl_window_create();
}

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#endif
}

void common_linux_setup();
static int emu_flycast_init();

static void emu_flycast_term()
{
	flycast_term();
	LogManager::Shutdown();
}

extern "C" int SDL_main(int argc, char *argv[])
{
    char *base_dir = getenv("DOJO_BASE_DIR");
    char *home = getenv("HOME");
    if (home != NULL || base_dir != NULL)
    {
        std::string config_dir;
        if (base_dir != NULL)
        {
            config_dir = std::string(base_dir) + "/";
        }
        else
        {
            config_dir = std::string(home) + "/.reicast/";
            if (!file_exists(config_dir))
                config_dir = std::string(home) + "/.flycast_dojo/";
            if (!file_exists(config_dir))
                config_dir = std::string(home) + "/Library/Application Support/Flycast Dojo/";
        }

        /* Different config folder for multiple instances */
        int instanceNumber = (int)[[NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.blueminder.FlycastDojo"] count];
		if (instanceNumber > 1)
		{
			config_dir += std::to_string(instanceNumber) + "/";
			[[NSApp dockTile] setBadgeLabel:@(instanceNumber).stringValue];
		}

        mkdir(config_dir.c_str(), 0755); // create the directory if missing
        set_user_config_dir(config_dir);
        add_system_data_dir(config_dir);

        NSBundle *thisBundle = [NSBundle mainBundle];
        NSString *userpath = [NSString stringWithCString:config_dir.c_str() encoding:[NSString defaultCStringEncoding]];

        std::string roms_json = config_dir + "flycast_roms.json";
        if (!file_exists(roms_json))
        {
            NSString *saveFilePath = [userpath stringByAppendingPathComponent:@"flycast_roms.json"];
            NSFileManager *fileManager = [[NSFileManager alloc] init];
            if ([fileManager fileExistsAtPath:userpath] == NO)
                [fileManager createDirectoryAtPath:userpath withIntermediateDirectories:YES attributes:nil error:nil];

            [fileManager copyItemAtPath:[thisBundle pathForResource:@"flycast_roms" ofType:@"json"] toPath:saveFilePath error:NULL];
        }

        std::string data_dir = config_dir + "data";
        if (!file_exists(data_dir))
        {
            NSString *saveFilePath = [userpath stringByAppendingPathComponent:@"data"];
            NSFileManager *fileManager = [[NSFileManager alloc] init];
            if ([fileManager fileExistsAtPath:userpath] == NO)
                [fileManager createDirectoryAtPath:userpath withIntermediateDirectories:YES attributes:nil error:nil];

            [fileManager copyItemAtPath:[thisBundle pathForResource:@"data" ofType:@""] toPath:saveFilePath error:NULL];
        }
        set_user_data_dir(data_dir + "/");

        std::string replays_dir = config_dir + "replays/";
        mkdir(replays_dir.c_str(), 0755);

        std::string roms_dir = config_dir + "ROMs/";
        mkdir(roms_dir.c_str(), 0755);
    }
    else
    {
        set_user_config_dir("./");
        set_user_data_dir("./");
    }
    // Add bundle resources path
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        add_system_data_dir(std::string(path) + "/");
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

	emu_flycast_init();

	mainui_loop();

	emu_flycast_term();
	os_UninstallFaultHandler();
	sdl_window_destroy();

	return 0;
}

static int emu_flycast_init()
{
	LogManager::Init();
	common_linux_setup();
	NSArray *arguments = [[NSProcessInfo processInfo] arguments];
	unsigned long argc = [arguments count];
	char **argv = (char **)malloc(argc * sizeof(char*));
	int paramCount = 0;
	for (unsigned long i = 0; i < argc; i++)
	{
		const char *arg = [[arguments objectAtIndex:i] UTF8String];
		if (!strncmp(arg, "-psn_", 5))
			// ignore Process Serial Number argument on first launch
			continue;
		argv[paramCount++] = strdup(arg);
	}
	
	int rc = flycast_init(paramCount, argv);
	
	for (unsigned long i = 0; i < paramCount; i++)
		free(argv[i]);
	free(argv);
	
	return rc;
}

std::string os_Locale(){
    return [[[NSLocale preferredLanguages] objectAtIndex:0] UTF8String];
}

std::string os_PrecomposedString(std::string string){
    return [[[NSString stringWithUTF8String:string.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}
