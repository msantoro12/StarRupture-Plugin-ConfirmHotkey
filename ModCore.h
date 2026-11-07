#pragma once

#include "plugin_interface.h"

class ModCore
{
public:
    static bool ResolveFields(IPluginSelf* self, IPluginHookScanner* scanner);
    static void InitSdkGlobals(IPluginSelf* self);
    static bool Initialize(IPluginSelf* self);
    static void Shutdown();

private:
    static void OnConfirmHotkey(EModKey key, EModKeyEvent event);

    static IPluginSelf* s_self;
    static char         s_keyName[64];
};
