/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * toolboxcmd-stat.c --
 *
 *     Various stat operations for toolbox-cmd
 */

#include <time.h>
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"


/*
 *-----------------------------------------------------------------------------
 *
 * OpenHandle  --
 *
 *      Opens a VMGuestLibHandle.
 *
 * Results:
 *      0 on success.
 *      EX_UNAVAILABLE on failure to open handle.
 *      EX_TEMPFAIL on failure to update info.
 *
 * Side effects:
 *      Prints to stderr and exits on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
OpenHandle(VMGuestLibHandle *glHandle, // OUT: The guestlib handle
           VMGuestLibError *glError)   // OUT: The errors when opening the handle
{
   *glError = VMGuestLib_OpenHandle(glHandle);
   if (*glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.openhandle.failed,
                            "OpenHandle failed: %s\n"),
                        VMGuestLib_GetErrorText(*glError));
      return EX_UNAVAILABLE;
   }
   *glError = VMGuestLib_UpdateInfo(*glHandle);
   if (*glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.update.failed,
                            "UpdateInfo failed: %s\n"),
                        VMGuestLib_GetErrorText(*glError));
      return EX_TEMPFAIL;
   }
   return 0;  // We don't return EXIT_SUCCESSS to indicate that this is not
              // an exit code

}


/*
 *-----------------------------------------------------------------------------
 *
 * StatProcessorSpeed  --
 *
 *      Gets the Processor Speed.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatProcessorSpeed(void)
{
   uint32 speed;
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETMHZ;
   Backdoor(&bp);
   speed = bp.out.ax.word;
   if (speed < 0) {
      ToolsCmd_PrintErr("%s",
                        SU_(stat.getspeed.failed, "Unable to get processor speed.\n"));
      return EX_TEMPFAIL;
   }
   printf("%u MHz\n", speed);
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatHostTime  --
 *
 *      Gets the host machine's time.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatHostTime(void)
{
   int64 hostSecs;
   int64 hostUsecs;
   time_t sec;
   char buf[256];
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL;
   Backdoor(&bp);
   if (bp.out.ax.word == BDOOR_MAGIC) {
      hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
   } else {
      /* Falling back to older command. */
      bp.in.cx.halfs.low = BDOOR_CMD_GETTIME;
      Backdoor(&bp);
      hostSecs = bp.out.ax.word;
   }
   hostUsecs = bp.out.bx.word;

   if (hostSecs <= 0) {
      ToolsCmd_PrintErr("%s",
                        SU_(stat.gettime.failed, "Unable to get host time.\n"));
      return EX_TEMPFAIL;
   }

   sec = hostSecs + (hostUsecs / 1000000);
   if (strftime(buf, sizeof buf, "%d %b %Y %H:%M:%S", localtime(&sec)) == 0) {
      ToolsCmd_PrintErr("%s",
                        SU_(stat.formattime.failed, "Unable to format host time.\n"));
      return EX_TEMPFAIL;
   }
   printf("%s\n", buf);
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetSessionID --
 *
 *      Gets the Session ID for the virtual machine
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetSessionID(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint64 session;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetSessionId(glHandle, &session);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.getsession.failed,
                            "Failed to get session ID: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("0x%"FMT64"x\n", session);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetMemoryBallooned  --
 *
 *      Retrieves memory ballooned.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetMemoryBallooned(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memBallooned;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemBalloonedMB(glHandle, &memBallooned);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.balloon.failed,
                            "Failed to get ballooned memory: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", memBallooned);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetMemoryReservation  --
 *
 *      Retrieves min memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetMemoryReservation(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32  memReservation;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemReservationMB(glHandle, &memReservation);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.memres.failed,
                            "Failed to get memory reservation: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memReservation);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetMemorySwapped  --
 *
 *      Retrieves swapped memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *      If OpenHandle fails the program exits.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetMemorySwapped(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memSwapped;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemSwappedMB(glHandle, &memSwapped);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.memswap.failed,
                            "Failed to get swapped memory: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memSwapped);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetMemoryLimit --
 *
 *      Retrieves max memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetMemoryLimit(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memLimit;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemLimitMB(glHandle, &memLimit);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.maxmem.failed,
                            "Failed to get memory limit: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memLimit);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetCpuReservation  --
 *
 *      Retrieves cpu min speed.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetCpuReservation(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 cpuReservation;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetCpuReservationMHz(glHandle, &cpuReservation);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.cpumin.failed,
                            "Failed to get CPU minimum: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", cpuReservation);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatGetCpuLimit  --
 *
 *      Retrieves cpu max speed .
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatGetCpuLimit(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 cpuLimit;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetCpuLimitMHz(glHandle, &cpuLimit);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      ToolsCmd_PrintErr(SU_(stat.cpumax.failed,
                            "Failed to get CPU limit: %s\n"),
                        VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", cpuLimit);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_Command --
 *
 *      Handle and parse stat commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit codes on errors.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_Command(char **argv,      // IN: Command line arguments
             int argc,         // IN: Length of command line arguments
             gboolean quiet)   // IN
{
   if (toolbox_strcmp(argv[optind], "hosttime") == 0) {
      return StatHostTime();
   } else if (toolbox_strcmp(argv[optind], "sessionid") == 0) {
      return StatGetSessionID();
   } else if (toolbox_strcmp(argv[optind], "balloon") == 0) {
      return StatGetMemoryBallooned();
   } else if (toolbox_strcmp(argv[optind], "swap") == 0) {
      return StatGetMemorySwapped();
   } else if (toolbox_strcmp(argv[optind], "memlimit") == 0) {
      return StatGetMemoryLimit();
   } else if (toolbox_strcmp(argv[optind], "memres") == 0) {
      return StatGetMemoryReservation();
   } else if (toolbox_strcmp(argv[optind], "cpures") == 0) {
      return StatGetCpuReservation();
   } else if (toolbox_strcmp(argv[optind], "cpulimit") == 0) {
      return StatGetCpuLimit();
   } else if (toolbox_strcmp(argv[optind], "speed") == 0) {
      return StatProcessorSpeed();
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_Help --
 *
 *      Prints the help for the stat command.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Stat_Help(const char *progName, // IN: The name of the program obtained from argv[0]
          const char *cmd)      // IN
{
   g_print(SU_(help.stat, "%s: print useful guest and host information\n"
                          "Usage: %s %s <subcommand>\n\n"
                          "Subcommands:\n"
                          "   hosttime: print the host time\n"
                          "   speed: print the CPU speed in MHz\n"
                          "ESX guests only subcommands:\n"
                          "   sessionid: print the current session id\n"
                          "   balloon: print memory ballooning information\n"
                          "   swap: print memory swapping information\n"
                          "   memlimit: print memory limit information\n"
                          "   memres: print memory reservation information\n"
                          "   cpures: print CPU reservation information\n"
                          "   cpulimit: print CPU limit information\n"),
           cmd, progName, cmd);
}

