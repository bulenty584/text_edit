#ifndef HISTORY_H
#define HISTORY_H

#include "common.h"

void historyInit(void);
void historyFree(void);
void historyRecord(EditOperation op);
void historyUndo(void);
void historyRedo(void);

#endif
