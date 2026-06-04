#pragma once
#include <stdint.h>
#include <mach-o/dyld.h>
#include <string.h>

static uintptr_t GetSlide() {
    static uintptr_t slide = 0;
    if (!slide) {
        for (uint32_t i = 0; i < _dyld_image_count(); i++) {
            const char* name = _dyld_get_image_name(i);
            if (name && strstr(name, "RobloxPlayer")) {
                slide = _dyld_get_image_vmaddr_slide(i);
                break;
            }
        }
    }
    return slide;
}

#define rebase(x) (x + GetSlide())

namespace offsets {
    inline uintptr_t GetGlobalState = rebase(0x10180A228);
    inline uintptr_t luau_load      = rebase(0x1042384CA);
    inline uintptr_t spawn          = rebase(0x10175538A);
    inline uintptr_t print          = rebase(0x1001BED44);
}

namespace RBX::Offsets {
    namespace Luau {
        inline uintptr_t luau_load         = rebase(0x1042384CA);
        inline uintptr_t GetGlobalState    = rebase(0x10180A228);
        inline uintptr_t PushInstance      = rebase(0x101836325);
        inline uintptr_t OpCodeLookupTable = rebase(0x10559284E);
    }

    namespace ScriptContext {
        inline uintptr_t CachedScriptContext = 0;
        inline uintptr_t CachedDataModel     = 0;
    }

    namespace VisualEngine {
        inline uintptr_t Pointer      = rebase(0x106411318);
        inline uintptr_t FakeDatamodel = 0xA90;
        inline uintptr_t Datamodel     = 0x1D0;
    }

    namespace TaskScheduler {
        inline uintptr_t Pointer   = rebase(0x1066B1DB0);
        inline uintptr_t JobsStart = 0x120;
        inline uintptr_t JobsEnd   = 0x128;
        inline uintptr_t JobName   = 0x10;
    }

    namespace Instance {
        inline constexpr uintptr_t Children        = 0x130;
        inline constexpr uintptr_t ChildrenEnd     = 0x138;
        inline constexpr uintptr_t ClassDescriptor = 0x18;
        inline constexpr uintptr_t ClassName       = 0x8;
        inline constexpr uintptr_t Parent          = 0x70;
        inline constexpr uintptr_t This            = 0x8;
        inline constexpr uintptr_t ScriptContext   = 0x28;
    }

    namespace Datamodel {
        inline constexpr uintptr_t PlaceId          = 0x1a0; // TODO: incorrect
        inline constexpr uintptr_t GameLoadedStatus = 0x638;
        inline constexpr uintptr_t ScriptContext    = 0x440;
    }

    namespace LocalScript {
        inline constexpr uintptr_t Hash = 0x1b8; // TODO: incorrect
    }

    namespace ModuleScript {
        inline constexpr uintptr_t Hash = 0x160; // TODO: incorrect
    }
}