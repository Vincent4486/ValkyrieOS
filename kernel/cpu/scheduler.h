// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cpu/process.h>

void Scheduler_Initialize();

void Scheduler_RegisterProcess(Process *process);
void Scheduler_UnregisterProcess(Process *process);

void Scheduler_Schedule();

void Scheduler_SetProcessState();
void Scheduler_GetNextRunnableProcess();
