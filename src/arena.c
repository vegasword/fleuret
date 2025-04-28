#define KB 1024LL
#define MB KB*1000
#define GB MB*1000

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

typedef struct Arena {
  size_t cur;
  size_t prev;
  size_t capacity;
  uc *data;
} Arena;

typedef struct TmpArena {
  size_t prev;
  size_t cur;
  Arena *arena;
} TmpArena;

i32 IsPowerOfTwo(uintptr_t x)
{ 
  return (x & (x - 1)) == 0;
}

uintptr_t AlignForward(uintptr_t ptr, size_t align)
{
  uintptr_t p, a, modulo;
  assert(IsPowerOfTwo(align));
  p = ptr;
  a = (uintptr_t)align;
  modulo = p & (a - 1);
  if (modulo != 0) p += a - modulo;
  return p;
}

void *AllocAlign(Arena *arena, size_t size, size_t align)
{
  uintptr_t curr_ptr = (uintptr_t)arena->data + (uintptr_t)arena->cur;
  uintptr_t offset = AlignForward(curr_ptr, align);
  offset -= (uintptr_t)arena->data;

  if (offset + size <= arena->capacity)
  {
    void *ptr = &arena->data[offset];
    arena->prev = offset;
    arena->cur = offset + size;
    memset(ptr, 0, size);
    return ptr;
  }
  else
  {
    return NULL;
  }
}

void *Alloc(Arena *arena, size_t size) 
{
  return AllocAlign(arena, size, DEFAULT_ALIGNMENT);
}

void InitArena(Arena *arena, void *backBuffer, size_t backBufferLength)
{
  arena->data = (uc *)backBuffer;
  arena->capacity = backBufferLength;
  arena->cur = 0;
  arena->prev = 0;
}

void DestroyArena(Arena *arena)
{
  memset(arena->data, 0, arena->capacity);
  arena->cur = 0;
  arena->prev = 0;
}

void TmpArenaPush(TmpArena *tmp, Arena *src)
{
  tmp->arena = src;
  tmp->prev = src->prev;
  tmp->cur = src->cur;
}

void TmpArenaPop(TmpArena *tmp)
{
  tmp->arena->prev = tmp->prev;
  tmp->arena->cur = tmp->cur;
}
