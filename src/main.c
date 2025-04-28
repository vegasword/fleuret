#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "assert.h"

#include "typedefs.c"
#include "arena.c"

#include <Windows.h>

#define READ_BE16(p) (u16)((((u8*)(p))[0] << 8) | (((u8*)(p))[1]))
#define READ_BE32(p) (u32)((((u8*)(p))[0] << 24) | (((u8*)(p))[1] << 16) | (((u8*)(p))[2] << 8) | (((u8*)(p))[3]))
#define PTR_MOVE(p, a) ((p) += (a))
#define READ_BE16_MOVE(p) (READ_BE16((p))); (PTR_MOVE((p), 2))
#define READ_BE32_MOVE(p) (READ_BE32((p))); (PTR_MOVE((p), 4))

#define TRUETYPE_SCALER_TYPE 0x74727565
#define TRUETYPE_SCALER_TYPE_EX 0x00010000
#define OPENTYPE_SCALER_TYPE 0x4F54544F
#define POSTSCRIPT_SCALER_TYPE 0x74797031

typedef struct {
  u32 scalerType;
  u16 tablesCount;
  u16 searchRange;
  u16 entrySelector;
  u16 rangeShift;
} OffsetSubtable;

typedef struct {
  union {
    char string[4];
    u32 value;
  } tag;
  u32 checkSum;
  u32 offset;
  u32 length;
} TableDirectory;

typedef struct {
  OffsetSubtable offsetSubtable;
  TableDirectory *tableDirectories;
} FontDirectory;

FontDirectory *ReadFontDirectory(Arena *arena, char **buffer)
{
  char *p = *buffer;
  FontDirectory *fontDirectory = (FontDirectory  *)Alloc(arena, sizeof(FontDirectory));
  
  OffsetSubtable *offsetSubtable = &fontDirectory->offsetSubtable;
  offsetSubtable->scalerType = READ_BE32_MOVE(p);
  offsetSubtable->tablesCount = READ_BE16_MOVE(p);
  offsetSubtable->searchRange = READ_BE16_MOVE(p);
  offsetSubtable->entrySelector = READ_BE16_MOVE(p);
  offsetSubtable->rangeShift = READ_BE16_MOVE(p);
  
  i32 tablesCount = offsetSubtable->tablesCount;
  fontDirectory->tableDirectories = (TableDirectory *)Alloc(arena, tablesCount * sizeof(TableDirectory));
  TableDirectory *tableDirectories = fontDirectory->tableDirectories;
  for (i32 i = 0; i < tablesCount; ++i)
  {
    TableDirectory *tableDirectory = &tableDirectories[i];
    tableDirectory->tag.value = READ_BE32_MOVE(p);
    tableDirectory->checkSum = READ_BE32_MOVE(p);
    tableDirectory->offset = READ_BE32_MOVE(p);
    tableDirectory->length = READ_BE32_MOVE(p);
  }
  
  *buffer = p;
  fontDirectory->offsetSubtable = *offsetSubtable;
  
  return fontDirectory;
}

void PrintFontDirectory(FontDirectory *fontDirectory)
{
  printf("Scaler type: ");
  switch (fontDirectory->offsetSubtable.scalerType)
  {
    case TRUETYPE_SCALER_TYPE: case TRUETYPE_SCALER_TYPE_EX: printf("TRUETYPE\n"); break;
    case OPENTYPE_SCALER_TYPE: printf("OPENTYPE\n"); break;
    case POSTSCRIPT_SCALER_TYPE: printf("POSTSCRIPT\n"); break;
    default: printf("\n"); break;
  }
  
  TableDirectory *tableDirectories = fontDirectory->tableDirectories;
  i32 tablesCount = fontDirectory->offsetSubtable.tablesCount;
  for (i32 i = 0; i < tablesCount; ++i)
  {
    TableDirectory *tableDirectory = &tableDirectories[i];
    printf("%c%c%c%c %d %d\n",
           tableDirectory->tag.string[3],
           tableDirectory->tag.string[2],
           tableDirectory->tag.string[1],
           tableDirectory->tag.string[0],
           tableDirectory->length,
           tableDirectory->offset);
  }
}

char *ReadWholeFile(Arena *arena, char *filePath, i32 *fileSize)
{
  FILE *file = fopen(filePath, "rb");
  if (file)
  {
    fseek(file, 0, SEEK_END);
    *fileSize = (i32)ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = (char *)Alloc(arena, *fileSize + 1);
    i32 success = fread(content, *fileSize, 1, file);
    fclose(file);
    return success ? content : NULL;
  }
  return NULL;
}

int main()
{
  Arena arena;
  InitArena(&arena, VirtualAlloc(NULL, 2*GB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE), 2*GB);

  i32 fileSize = 0;
  char *buffer = ReadWholeFile(&arena, "envy.ttf", &fileSize);
  if (buffer)
  {
    FontDirectory *fontDirectory = ReadFontDirectory(&arena, &buffer);
    PrintFontDirectory(fontDirectory);
  }
  
  return 0;
}
