
#include <ultra64.h>
#include <sched.h>

#include "defs.h"
#include "graphics/graphics.h"
#include "util/rom.h"
#include "scene/scene.h"
#include "util/time.h"
#include "util/memory.h"
#include "string.h"
#include "controls/controller.h"
#include "scene/dynamic_scene.h"
#include "audio/soundplayer.h"
#include "audio/audio.h"

#include "levels/levels.h"

#ifdef WITH_DEBUGGER
#include "../debugger/debugger.h"
#endif

static OSThread gameThread;
static OSThread initThread;

u64    mainStack[STACKSIZEBYTES/sizeof(u64)];
static u64 gameThreadStack[STACKSIZEBYTES/sizeof(u64)];
static u64 initThreadStack[STACKSIZEBYTES/sizeof(u64)];

static void gameProc(void *);
static void initProc(void *);

static OSMesg           PiMessages[DMA_QUEUE_SIZE];
static OSMesgQueue      PiMessageQ;

OSMesgQueue      gfxFrameMsgQ;
static OSMesg           gfxFrameMsgBuf[MAX_FRAME_BUFFER_MESGS];
static OSScClient       gfxClient;


OSSched scheduler;
u64            scheduleStack[OS_SC_STACKSIZE/8];
OSMesgQueue	*schedulerCommandQueue;

OSPiHandle	*gPiHandle;

void main(void *arg) {
    osInitialize();

    gPiHandle = osCartRomInit();

    osCreateThread(
        &initThread, 
        1, 
        initProc, 
        NULL,
        (void *)(initThreadStack+(STACKSIZEBYTES/sizeof(u64))), 
		(OSPri)INIT_PRIORITY
    );

    osStartThread(&initThread);
}

static void initProc(void* arg) {
    osCreatePiManager(
        (OSPri) OS_PRIORITY_PIMGR, 
        &PiMessageQ,
        PiMessages,
        DMA_QUEUE_SIZE
    );

    osCreateThread(
        &gameThread, 
        6, 
        gameProc, 
        0, 
        gameThreadStack + (STACKSIZEBYTES/sizeof(u64)),
        (OSPri)GAME_PRIORITY
    );

    osStartThread(&gameThread);

    osSetThreadPri(NULL, 0);
    for(;;);
}

struct Scene gScene;

extern OSMesgQueue dmaMessageQ;

extern char _heapStart[];

#define FRAME_SKIP  0

static void gameProc(void* arg) {
    u8 schedulerMode = OS_VI_NTSC_LPF1;

	switch (osTvType) {
		case 0: // PAL
			schedulerMode = HIGH_RES ? OS_VI_PAL_HPF1 : OS_VI_PAL_LPF1;
			break;
		case 1: // NTSC
			schedulerMode = HIGH_RES ? OS_VI_NTSC_HPF1 : OS_VI_NTSC_LPF1;
			break;
		case 2: // MPAL
            schedulerMode = HIGH_RES ? OS_VI_MPAL_HPF1 : OS_VI_MPAL_LPF1;
			break;
	}

    osCreateScheduler(
        &scheduler,
        (void *)(scheduleStack + OS_SC_STACKSIZE/8),
        SCHEDULER_PRIORITY,
        schedulerMode,
        1
    );

    schedulerCommandQueue = osScGetCmdQ(&scheduler);

    osCreateMesgQueue(&gfxFrameMsgQ, gfxFrameMsgBuf, MAX_FRAME_BUFFER_MESGS);
    osScAddClient(&scheduler, &gfxClient, &gfxFrameMsgQ);

	osViSetSpecialFeatures(OS_VI_GAMMA_OFF |
			OS_VI_GAMMA_DITHER_OFF |
			OS_VI_DIVOT_OFF |
			OS_VI_DITHER_FILTER_OFF);

    u32 pendingGFX = 0;
    u32 drawBufferIndex = 0;
    u8 frameControl = 0;
    u8 inputIgnore = 6;

    u16* memoryEnd = graphicsLayoutScreenBuffers((u16*)PHYS_TO_K0(osMemSize));

    gAudioHeapBuffer = (u8*)memoryEnd - AUDIO_HEAP_SIZE;

    memoryEnd = (u16*)gAudioHeapBuffer;

    heapInit(_heapStart, memoryEnd);
    romInit();

    dynamicSceneInit();
    contactSolverInit(&gContactSolver);
    levelLoad(0);
    sceneInit(&gScene);
    controllersInit();
    initAudio();
    soundPlayerInit();
#ifdef WITH_DEBUGGER
    OSThread* debugThreads[2];
    debugThreads[0] = &gameThread;
    gdbInitDebugger(gPiHandle, &dmaMessageQ, debugThreads, 1);
#endif

    while (1) {
        OSScMsg *msg = NULL;
        osRecvMesg(&gfxFrameMsgQ, (OSMesg*)&msg, OS_MESG_BLOCK);
        
        switch (msg->type) {
            case (OS_SC_RETRACE_MSG):
                // control the framerate
                frameControl = (frameControl + 1) % (FRAME_SKIP + 1);
                if (frameControl != 0) {
                    break;
                }

                static int renderSkip = 1;

                if (pendingGFX < 2 && !renderSkip) {
                    graphicsCreateTask(&gGraphicsTasks[drawBufferIndex], (GraphicsCallback)sceneRender, &gScene);
                    drawBufferIndex = drawBufferIndex ^ 1;
                    ++pendingGFX;
                } else if (renderSkip) {
                    --renderSkip;
                }

                controllersTriggerRead();
                if (inputIgnore) {
                    --inputIgnore;
                } else {
                    sceneUpdate(&gScene);
                }
                timeUpdateDelta();
                soundPlayerUpdate();

                break;

            case (OS_SC_DONE_MSG):
                --pendingGFX;
                break;
            case (OS_SC_PRE_NMI_MSG):
                pendingGFX += 2;
                break;
            case SIMPLE_CONTROLLER_MSG:
                controllersUpdate();
                break;
        }
    }
}