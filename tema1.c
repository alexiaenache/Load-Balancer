
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STRING_SIZE 100
typedef struct miniblock_t {
  uint64_t start_address;  // adresa de început a zonei, un indice din arenă
  size_t size;             // size-ul miniblock-ului
  uint8_t perm;            // permisiunile asociate zonei, by default RW-
  void* rw_buffer;  // buffer-ul de date, folosit pentru opearțiile de read() și
                    // write()
  struct miniblock_t *next, *prev;
} miniblock_t;
typedef struct miniblock_list {
  miniblock_t* head;
  miniblock_t* last;
  unsigned int data_size;
  unsigned int size;
} miniblock_list;
typedef struct block_t {
  uint64_t start_address;  // adresa de început a zonei, un indice din arenă
  size_t size;  // dimensiunea totală a zonei, suma size-urilor miniblock-urilor
  void* miniblock_list;  // lista de miniblock-uri adiacente
  struct block_t *next, *prev;
} block_t;
typedef struct {
  block_t* head;
  block_t* last;
  unsigned int data_size;
  unsigned int size;
} list_t;

typedef struct arena_t {
  uint64_t arena_size, free_size;
  list_t* alloc_list;
  int no_miniblocks;
} arena_t;
list_t* create_block_list() {
  list_t* list = malloc(sizeof(list_t));
  if (list == NULL) {
    printf("Failed to alloc list");
    return NULL;
  }
  list->size = 0;
  list->head = NULL;
  list->last = NULL;
  return list;
}
arena_t* alloc_arena(const uint64_t size) {
  arena_t *arena, *aux;
  aux = malloc(sizeof(arena_t));
  if (aux == NULL) {
    printf("Failed to alloc memory for the arena");
    return NULL;
  } else {
    arena = aux;
  }
  arena->arena_size = size;
  arena->free_size = size;
  arena->no_miniblocks = 0;
  arena->alloc_list = create_block_list();
  return arena;
}

miniblock_list* create_miniblock_list() {
  miniblock_list* list = malloc(sizeof(miniblock_list));
  list->size = 0;
  list->head = NULL;
  list->last = NULL;
  return list;
}
void free_mem_block(block_t* block) {
  free(((miniblock_list*)block->miniblock_list));
  free(block);
}
void free_mem_miniblock(miniblock_t* miniblock) {
  free(miniblock->rw_buffer);
  free(miniblock);
}
int check_memory(list_t* list, uint16_t start_address, size_t size,
                 arena_t* arena) {
  if (start_address >= arena->arena_size) {
    printf("The allocated address is outside the size of arena\n");
    return 0;
  }
  if (start_address + size > arena->arena_size) {
    printf("The end address is past the size of the arena\n");
    return 0;
  }
  block_t* curr;
  if (list->head == NULL) {
    return 1;
  }
  curr = list->head;
  if (start_address <= curr->start_address) {
    if (start_address + size > curr->start_address) {
      printf("This zone was already allocated.\n");
      return 0;
    }
  }
  while (curr->next != NULL) {
    if (curr->start_address <= start_address &&
        curr->next->start_address >= start_address) {
      if (curr->start_address + curr->size > start_address ||
          start_address + size > curr->next->start_address) {
        printf("This zone was already allocated.\n");
        return 0;
      }
    }
    curr = curr->next;
  }

  if (start_address >= curr->start_address) {
    if (start_address < curr->start_address + curr->size) {
      printf("This zone was already allocated.\n");
      return 0;
    }
  }
  return 1;
}
miniblock_t* create_miniblock(uint64_t start_address, size_t size) {
  miniblock_t* aux;
  aux = malloc(sizeof(miniblock_t));
  if (aux == NULL) {
    printf("Failed to alloc miniblock");
    return NULL;
  }
  aux->start_address = start_address;
  aux->size = size;
  aux->next = NULL;
  aux->prev = NULL;
  aux->rw_buffer = malloc(sizeof(char) * size);
  aux->perm = 6;
  return aux;
}
block_t* remove_block(list_t* list, unsigned int n) {
  block_t* aux;
  if (list->size == 1) {
    aux = list->head;
    list->head = NULL;
  } else if (n == 0) {
    aux = list->head;
    list->head->next->prev = NULL;
    list->head = list->head->next;
  } else if (n >= list->size - 1) {
    aux = list->last;
    list->last->prev->next = NULL;
    list->last = list->last->prev;
  } else {
    block_t* curr = list->head;
    for (unsigned int i = 0; i < n - 1; i++) {
      curr = curr->next;
    }
    aux = curr->next;
    curr->next = aux->next;
    aux->next->prev = aux->prev;
  }
  list->size--;
  return aux;
}
int check_neighbors(list_t* list, uint64_t start_address, size_t size) {
  if (list->head == NULL) {
    return 1;
  }
  block_t* curr = list->head;
  if (list->head->next == NULL) {
    if (start_address + size == curr->start_address) {
      miniblock_t* aux;
      aux = create_miniblock(start_address, size);
      aux->next = ((miniblock_list*)curr->miniblock_list)->head;
      ((miniblock_list*)curr->miniblock_list)->head->prev = aux;
      ((miniblock_list*)curr->miniblock_list)->head = aux;
      ((miniblock_list*)curr->miniblock_list)->size++;
      curr->start_address = start_address;
      curr->size += size;
      return 0;
    }
  }
  unsigned int i = 0;
  while (curr->next != NULL) {
    if (curr->start_address + curr->size == start_address &&
        curr->next->start_address == start_address + size) {
      miniblock_t* aux;
      aux = create_miniblock(start_address, size);
      ((miniblock_list*)curr->miniblock_list)->last->next = aux;
      aux->prev = ((miniblock_list*)curr->miniblock_list)->last;
      ((miniblock_list*)curr->miniblock_list)->last = aux;
      ((miniblock_list*)curr->next->miniblock_list)->head->prev = aux;
      aux->next = ((miniblock_list*)curr->next->miniblock_list)->head;
      ((miniblock_list*)curr->miniblock_list)->last =
          ((miniblock_list*)curr->next->miniblock_list)->last;
      ((miniblock_list*)curr->miniblock_list)->size++;
      ((miniblock_list*)curr->miniblock_list)->size +=
          ((miniblock_list*)curr->next->miniblock_list)->size;
      curr->size += size;
      curr->size += curr->next->size;
      block_t* erase = remove_block(list, i + 1);
      free_mem_block(erase);
      return 0;
    } else if (curr->start_address + curr->size == start_address) {
      miniblock_t* aux;
      aux = create_miniblock(start_address, size);
      ((miniblock_list*)curr->miniblock_list)->last->next = aux;
      aux->prev = ((miniblock_list*)curr->miniblock_list)->last;
      ((miniblock_list*)curr->miniblock_list)->last = aux;
      ((miniblock_list*)curr->miniblock_list)->size++;
      curr->size += size;
      return 0;
    } else if (curr->next->start_address == start_address + size) {
      miniblock_t* aux;
      aux = create_miniblock(start_address, size);
      ((miniblock_list*)curr->next->miniblock_list)->head->prev = aux;
      aux->next = ((miniblock_list*)curr->next->miniblock_list)->head;
      ((miniblock_list*)curr->next->miniblock_list)->head = aux;
      ((miniblock_list*)curr->next->miniblock_list)->size++;
      curr->next->start_address = start_address;
      curr->next->size += size;
      return 0;
    }
    curr = curr->next;
    i++;
  }
  if (curr->start_address + curr->size == start_address) {
    miniblock_t* aux;
    aux = create_miniblock(start_address, size);
    ((miniblock_list*)curr->miniblock_list)->last->next = aux;
    aux->prev = ((miniblock_list*)curr->miniblock_list)->last;
    ((miniblock_list*)curr->miniblock_list)->last = aux;
    ((miniblock_list*)curr->miniblock_list)->size++;
    curr->size += size;
    return 0;
  }
  return 1;
}
block_t* create_block(const uint64_t start_address, size_t size) {
  block_t* block = malloc(sizeof(block_t));
  block->start_address = start_address;
  block->size = size;
  block->next = NULL;
  block->prev = NULL;
  block->miniblock_list = create_miniblock_list();

  return block;
}
void add_block(list_t* list, uint64_t start_address, size_t size,
               unsigned int n) {
  block_t* block = create_block(start_address, size);

  if (list->size == 0) {
    list->head = block;
    list->last = block;

  } else if (n == 0) {
    block->next = list->head;
    list->head->prev = block;
    list->head = block;
  } else if (n >= list->size - 1) {
    block->prev = list->last;
    list->last->next = block;
    list->last = block;
  } else {
    block_t* curr = list->head;
    for (unsigned int i = 0; i < n - 1; i++) {
      curr = curr->next;
    }
    block->next = curr->next;
    block->prev = curr;
    curr->next = block;
    block->next->prev = block;
  }
  list->size++;
}
void alloc_block(arena_t* arena, const uint64_t start_address,
                 const uint64_t size) {
  if (check_memory(arena->alloc_list, start_address, size, arena)) {
    arena->free_size -= size;
    arena->no_miniblocks++;
    if (check_neighbors(arena->alloc_list, start_address, size)) {
      block_t* block = create_block(start_address, size);
      ((miniblock_list*)block->miniblock_list)->head =
          create_miniblock(start_address, size);
      ((miniblock_list*)block->miniblock_list)->last =
          ((miniblock_list*)block->miniblock_list)->head;
      ((miniblock_list*)block->miniblock_list)->size = 1;
      if (((list_t*)arena->alloc_list)->size == 0) {
        arena->alloc_list->head = block;
        arena->alloc_list->last = block;
        arena->alloc_list->size++;
        return;
      }
      if (block->start_address < arena->alloc_list->head->start_address) {
        block->next = arena->alloc_list->head;
        arena->alloc_list->head->prev = block;
        arena->alloc_list->head = block;
        arena->alloc_list->size++;
        return;

      } else if (block->start_address >
                 arena->alloc_list->last->start_address) {
        arena->alloc_list->last->next = block;
        block->prev = arena->alloc_list->last;
        arena->alloc_list->last = block;
        arena->alloc_list->size++;
      } else {
        block_t* curr = arena->alloc_list->head;
        while (curr->next != NULL) {
          if (curr->start_address < block->start_address &&
              curr->next->start_address > block->start_address) {
            block->next = curr->next;
            block->prev = curr;
            curr->next = block;
            block->next->prev = block;
          }
          curr = curr->next;
        }
        arena->alloc_list->size++;
      }
    }
  }
}
miniblock_t* remove_miniblock(block_t* block, unsigned int n) {
  miniblock_list* list = (miniblock_list*)block->miniblock_list;
  miniblock_t* aux;
  if (list->size == 1) {
    aux = list->head;
    list->head = NULL;
  } else if (n == 0) {
    aux = list->head;
    list->head = list->head->next;
    block->start_address = list->head->start_address;
  } else if (n >= list->size - 1) {
    aux = list->last;
    list->last->prev->next = NULL;
    list->last = list->last->prev;

  } else {
    miniblock_t* curr = list->head;
    for (unsigned int i = 0; i < n - 1; i++) {
      curr = curr->next;
    }
    aux = curr->next;
    curr->next = aux->next;
    aux->next->prev = aux->prev;
  }
  list->size--;
  block->size -= aux->size;
  return aux;
}

miniblock_t* split_block(list_t* list, block_t* block, unsigned int i,
                         unsigned int j) {
  miniblock_t* curr = ((miniblock_list*)block->miniblock_list)->head;
  for (unsigned int k = 0; k < i - 1; k++) {
    curr = curr->next;
  }
  miniblock_t* aux = curr->next;
  uint64_t start_address = aux->next->start_address;
  size_t size = 0;
  unsigned int no = 0;
  curr = curr->next->next;
  while (curr->next != NULL) {
    size += curr->size;
    curr = curr->next;
    no++;
  }
  size += curr->size;
  no++;
  add_block(list, start_address, size, j + 1);
  ((miniblock_list*)block->next->miniblock_list)->head = aux->next;
  ((miniblock_list*)block->next->miniblock_list)->last = curr;
  ((miniblock_list*)block->next->miniblock_list)->size = no;
  miniblock_t* erase = remove_miniblock(block, i);
  ((miniblock_list*)block->miniblock_list)->last = erase->prev;
  ((miniblock_list*)block->miniblock_list)->last->next = NULL;
  ((miniblock_list*)block->miniblock_list)->size -= no;
  block->size -= size;
  return erase;
}
void free_block(arena_t* arena, const uint64_t start_address) {
  list_t* list = arena->alloc_list;
  block_t* curr = list->head;
  miniblock_t* aux = NULL;
  block_t* aux2 = NULL;
  unsigned int j = 0;
  while (curr != NULL) {
    if (start_address >= curr->start_address &&
        start_address <= (curr->start_address + curr->size)) {
      miniblock_t* currm = ((miniblock_list*)curr->miniblock_list)->head;
      unsigned int i = 0;
      while (currm != NULL) {
        if (currm->start_address == start_address) {
          arena->free_size += currm->size;
          if (i > 0 && i < ((miniblock_list*)curr->miniblock_list)->size - 1) {
            aux = split_block(list, curr, i, j);
          } else if (((miniblock_list*)curr->miniblock_list)->size == 1) {
            aux = remove_miniblock(curr, i);
            aux2 = remove_block(list, j);
          } else {
            aux = remove_miniblock(curr, i);
          }
          arena->no_miniblocks--;
        }
        currm = currm->next;
        i++;
      }
    }
    curr = curr->next;
    j++;
  }
  if (aux != NULL) {
    free_mem_miniblock(aux);
  } else {
    printf("Invalid address for free.\n");
  }
  if (aux2 != NULL) {
    free_mem_block(aux2);
  }
}
void write_in_more_miniblocks(miniblock_t* miniblock, uint64_t size,
                              int8_t* data) {
  long unsigned int count = 0;
  while (count < size && miniblock != NULL) {
    if (size - count > miniblock->size) {
      memcpy(miniblock->rw_buffer, data + count, miniblock->size);
      count += miniblock->size;
    } else {
      memcpy(miniblock->rw_buffer, data + count, size - count);
      count += size - count;
    }

    miniblock = miniblock->next;
  }
}
void write(arena_t* arena, const uint64_t address, const uint64_t size,
           int8_t* data) {
  list_t* list = arena->alloc_list;
  block_t* curr = list->head;
  int ok = 0;
  while (curr != NULL) {
    if (address >= curr->start_address &&
        address <= (curr->start_address + curr->size)) {
      miniblock_t* currm = ((miniblock_list*)curr->miniblock_list)->head;
      while (currm != NULL) {
        if (address == currm->start_address) {
          if (currm->size < size) {
            if (currm->next == NULL) {
              printf(
                  "Warning: size was bigger than the block size. Writing "
                  "%d "
                  "characters.\n",
                  (int)currm->size);
              data[currm->size] = '\0';
              ok = 1;
              memcpy(currm->rw_buffer, data, currm->size);
              break;
            } else {
              ok = 1;
              write_in_more_miniblocks(currm, size, data);
            }
          } else {
            ok = 1;
            memcpy(currm->rw_buffer, data, size);
          }
        } else if (address > currm->start_address &&
                   address < (currm->start_address + currm->size)) {
          if (address + size > currm->start_address + currm->size) {
            printf(
                "Warning: size was bigger than the block size. Writing "
                "%lu "
                "characters.\n",
                (currm->start_address + currm->size - address));
            uint64_t indx = currm->start_address + currm->size - address;
            data[indx] = '\0';
          }
          ok = 1;
          memcpy(currm->rw_buffer, data, size);
        }
        currm = currm->next;
      }
    }
    curr = curr->next;
  }
  if (ok == 0) {
    printf("Invalid address for write.\n");
  }
}
void read_from_more_miniblocks(miniblock_t* miniblock, uint64_t size) {
  long unsigned int count = 0;
  while (count < size && miniblock != NULL) {
    if (size - count > miniblock->size) {
      for (long unsigned int i = 0; i < miniblock->size; i++) {
        printf("%c", *((char*)miniblock->rw_buffer + i));
      }
      count += miniblock->size;
    } else {
      for (long unsigned int i = 0; i < size - count; i++) {
        printf("%c", *((char*)miniblock->rw_buffer + i));
      }
      count += miniblock->size;
    }
    miniblock = miniblock->next;
  }
  printf("\n");
}
void read(arena_t* arena, uint64_t address, uint64_t size) {
  list_t* list = arena->alloc_list;
  block_t* curr = list->head;
  int ok = 0;
  while (curr != NULL) {
    if (address >= curr->start_address &&
        address <= (curr->start_address + curr->size)) {
      miniblock_t* currm = ((miniblock_list*)curr->miniblock_list)->head;
      while (currm != NULL) {
        if (address == currm->start_address) {
          if (currm->size < size) {
            if (currm->next == NULL) {
              printf(
                  "Warning: size was bigger than the block size. Reading %lu "
                  "characters.\n",
                  currm->size);

              ok = 1;
              for (long unsigned int i = 0; i < currm->size; i++) {
                printf("%c", *((char*)currm->rw_buffer + i));
              }
              printf("\n");
            } else {
              ok = 1;
              read_from_more_miniblocks(currm, size);
            }
          } else {
            ok = 1;
            for (long unsigned int i = 0; i < size; i++) {
              printf("%c", *((char*)currm->rw_buffer + i));
            }
            printf("\n");
          }
        } else if (address > currm->start_address &&
                   address < currm->start_address + currm->size) {
          ok = 1;
          for (long unsigned int i = address - currm->start_address; i <= size;
               i++)
            printf("%c", *((char*)currm->rw_buffer + i));
          printf("\n");
        }
        currm = currm->next;
      }
    }
    curr = curr->next;
  }
  if (ok == 0) {
    printf("Invalid address for read.\n");
  }
}
void pmap(const arena_t* arena) {
  printf("Total memory: 0x%lX bytes\n", (arena->arena_size));
  printf("Free memory: 0x%lX bytes\n", arena->free_size);
  printf("Number of allocated blocks: %d\n", arena->alloc_list->size);
  printf("Number of allocated miniblocks: %d\n", arena->no_miniblocks);
  block_t* curr = arena->alloc_list->head;

  for (unsigned int i = 1; i <= arena->alloc_list->size; i++) {
    printf("\nBlock %d begin\n", i);
    printf("Zone: 0x%lX - 0x%lX\n", curr->start_address,
           (curr->start_address + curr->size));
    miniblock_t* currm = ((miniblock_list*)curr->miniblock_list)->head;
    for (unsigned int j = 1; j <= ((miniblock_list*)curr->miniblock_list)->size;
         j++) {
      printf("Miniblock %d:", j);
      printf("\t\t0x%lX\t\t-\t\t0x%lX\t\t", currm->start_address,
             (currm->start_address + currm->size));
      printf("| RW-\n");
      currm = currm->next;
    }
    printf("Block %d end\n", i);
    curr = curr->next;
  }
}
void dealloc_arena(arena_t* arena) {
  block_t* curr = arena->alloc_list->head;
  block_t* aux;
  for (unsigned int i = 0; i < arena->alloc_list->size; i++) {
    miniblock_t* currm = ((miniblock_list*)curr->miniblock_list)->head;
    miniblock_t* auxm;
    for (unsigned int j = 0; j < ((miniblock_list*)curr->miniblock_list)->size;
         j++) {
      auxm = currm;
      currm = currm->next;
      free_mem_miniblock(auxm);
    }
    aux = curr;
    curr = curr->next;
    free_mem_block(aux);
  }
  free(arena->alloc_list);
  free(arena);
}
void show_error(int nr) {
  for (int i = 0; i <= nr; i++) {
    printf("Invalid command. Please try again.\n");
  }
}

int main() {
  char command[STRING_SIZE];
  // signed char p[100];
  // char c;
  int nr = 0;
  char* aux;
  // signed char copy[100];
  uint64_t start_address, nr_bytes;
  size_t size;
  arena_t* arena;
  while (1) {
    // scanf("%s", command);
    fgets(command, 50, stdin);
    nr = 0;
    command[strlen(command) + 1] = '\0';
    // memcpy(copy, command, 50);
    for (long unsigned int i = 0; i < strlen(command); i++) {
      if (command[i] == ' ') {
        nr++;
      }
    }
    aux = strtok(command, " ");
    if (strcmp(command, "ALLOC_ARENA") == 0) {
      if (nr != 1) {
        show_error(nr);
        continue;
      }
      aux = strtok(NULL, " ");
      nr_bytes = atol(aux);
      // scanf("%lu", &nr_bytes);
      arena = alloc_arena(nr_bytes);
    } else if (strcmp(command, "ALLOC_BLOCK") == 0) {
      if (nr != 2) {
        show_error(nr);
        continue;
      }
      aux = strtok(NULL, " ");
      start_address = atol(aux);
      aux = strtok(NULL, " ");
      size = atoi(aux);
      // scanf("%lu", &start_address);
      // scanf("%lu", &size);
      alloc_block(arena, start_address, size);
    } else if (strcmp(command, "FREE_BLOCK") == 0) {
      if (nr != 1) {
        show_error(nr);
        continue;
      }
      aux = strtok(NULL, " ");
      start_address = atol(aux);
      // scanf("%lu", &start_address);
      free_block(arena, start_address);
    } else if (strcmp(command, "WRITE") == 0) {
      if (nr < 3) {
        show_error(nr);
        continue;
      }
      aux = strtok(NULL, " ");
      start_address = atol(aux);
      aux = strtok(NULL, " ");
      size = atoi(aux);
      aux = strtok(NULL, "\0");

      // scanf("%lu", &start_address);
      // scanf("%lu", &size);
      // scanf("%c", &c);
      // for (long unsigned int i = 0; i < size; i++) {
      //   scanf("%c", &p[i]);
      // }
      char aux2[50];
      long unsigned int count;
      if (strlen(aux) < size) {
        count = strlen(aux);
        while (count < size) {
          fgets(aux2, 50, stdin);
          aux2[strlen(aux2)] = '\0';
          count += strlen(aux2);
          strcat(aux, aux2);
        }
      }
      write(arena, start_address, size, (signed char*)aux);
    } else if (strcmp(command, "READ") == 0) {
      if (nr != 2) {
        show_error(nr);
        continue;
      }
      aux = strtok(NULL, " ");
      start_address = atol(aux);
      aux = strtok(NULL, " ");
      size = atoi(aux);
      // scanf("%lu", &start_address);
      // scanf("%lu", &size);
      read(arena, start_address, size);
    } else if (strncmp(command, "PMAP", 4) == 0) {
      if (nr != 0) {
        show_error(nr);
        continue;
      }
      pmap(arena);
    }
    // else if (strcmp(command, "MPROTECT") == 0) {
    // if (nr < 2) {
    //   show_error(nr);
    //   continue;
    // }
    // aux = strtok(NULL, " ");
    // start_address = atol(aux);
    // aux = strtok(NULL, " ");
    // while(aux != NULL) {
    //   aux = startok(NULL, " ");
    //   if(strncmp(aux, "PROT_NONE", 9) == 0) {
    //     perm += 0;
    //   } else
    // }
    // mprotect(arena, start_address, (signed char*)aux);
    else if (strncmp(command, "DEALLOC_ARENA", 13) == 0) {
      if (nr != 0) {
        show_error(nr);
        continue;
      }
      dealloc_arena(arena);
      break;
    } else {
      show_error(nr);
      // printf("Invalid command. Please try again.\n");
    }
  }
  return 0;
}