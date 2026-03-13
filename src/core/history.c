#include "history.h"
#include "common.h"
#include "editor.h"

static int stackPush(EditStack *s, EditOperation op) {
    if (s->len == s->cap) {
        int new_cap = s->cap ? s->cap * 2 : 128;
        EditOperation *p = realloc(s->items, (size_t)new_cap * sizeof(EditOperation));
        if (!p) return 0;
        s->items = p;
        s->cap = new_cap;
    }
    s->items[s->len++] = op;
    return 1;
}

static int stackPop(EditStack *s, EditOperation *op) {
    if (s->len == 0) return 0;
    *op = s->items[--s->len];
    return 1;
}

static void stackClear(EditStack *s) { s->len = 0;}

static void applyInsertCharAt(int row, int col, char c) {
    E.cy = row;
    E.cx = col;
    editorInsertChar((unsigned char) c);
}

static void applyDeleteCharAt(int row, int col) {
    E.cy = row;
    E.cx = col + 1; // backspace deletes col
    editorDeleteChar();
}

static void applySplitLine(int row, int col) {
    E.cy = row;
    E.cx = col;
    editorInsertNewline();
}

static void applyJoinLine(int row) {
    E.cy = row + 1;
    E.cx = 0;
    editorDeleteChar();
}

static void historyApply(const EditOperation *op, int inverse) {
    switch (op->kind) {
        case OP_INSERT_CHAR:
            if (inverse) applyDeleteCharAt(op->row, op->col);
            else applyInsertCharAt(op->row, op->col, op->ch);
            break;
        case OP_DELETE_CHAR:
            if (inverse) applyInsertCharAt(op->row, op->col, op->ch);
            else applyDeleteCharAt(op->row, op->col);
            break;
        case OP_SPLIT_LINE:
            if (inverse) applyJoinLine(op->row);
            else applySplitLine(op->row, op->col);
            break;
        case OP_JOIN_LINE:
            if (inverse) applySplitLine(op->row, op->col);
            else applyJoinLine(op->row);
            break;
    }
}

void historyInit(void) {
    E.undo_stack.items = NULL; E.undo_stack.len = 0; E.undo_stack.cap = 0;
    E.redo_stack.items = NULL; E.redo_stack.len = 0; E.redo_stack.cap = 0;
    E.replaying_history = 0;
}

void historyFree(void) {
    free(E.undo_stack.items);
    free(E.redo_stack.items);
    E.undo_stack.items = NULL; E.redo_stack.items = NULL;
    E.undo_stack.len = 0; E.undo_stack.cap = 0;
    E.redo_stack.len = 0; E.redo_stack.cap = 0;
}

void historyRecord(EditOperation op) {
    if (E.replaying_history) return;
    if (stackPush(&E.undo_stack, op)) stackClear(&E.redo_stack);
}

void historyUndo(void) {
    EditOperation op;
    if (!stackPop(&E.undo_stack, &op)) return;
    E.replaying_history = 1;
    historyApply(&op, 1);
    E.replaying_history = 0;
    (void)stackPush(&E.redo_stack, op);
}

void historyRedo(void) {
    EditOperation op;
    if (!stackPop(&E.redo_stack, &op)) return;
    E.replaying_history = 1;
    historyApply(&op, 0);
    E.replaying_history = 0;
    (void)stackPush(&E.undo_stack, op);
}
