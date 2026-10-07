/* GPLv2 (c) Airbus */
#include <debug.h>
#include <info.h>

extern info_t   *info;
extern uint32_t __kernel_start__;
extern uint32_t __kernel_end__;

void tp() {
   debug("kernel mem [%p - %p]\n", &__kernel_start__, &__kernel_end__);
   debug("MBI flags 0x%x\n", info->mbi->flags);

   // on recup les bornes de la table mmap
   multiboot_uint32_t current = info->mbi->mmap_addr;
   multiboot_uint32_t end_mmap = current + info->mbi->mmap_length;

   // parcours de la liste des regions mémoire
   while (current < end_mmap) {
      memory_map_t *entry = (memory_map_t *)current;

      // bornes de la plage (adresse de fin : addr + len - 1)
      multiboot_uint32_t start = (multiboot_uint64_t)entry->addr;
      multiboot_uint32_t end   = (multiboot_uint64_t)entry->addr + (multiboot_uint64_t)entry->len - 1;

      // chaîne de caractères du type de mémoire
      const char *type_str = "MULTIBOOT_MEMORY_RESERVED";
      if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
         type_str = "MULTIBOOT_MEMORY_AVAILABLE";
      }

      // affichage : [start - end] TYPE
      debug("[0x%x - 0x%x] %s\n", start, end, type_str);

      // pour l'avancement du pointeur : taille de l'entrée + 4 octets du champ size
      current += entry->size + sizeof(entry->size);
   }

   /* Q3 : test lecture/écriture */
   int *ptr_in_available_mem = (int *)0x1000; // Adresse en RAM libre (ex: 0x1000)
   debug("Available mem (0x1000): before: 0x%x ", *ptr_in_available_mem);
   *ptr_in_available_mem = 0xaaaaaaaa;
   debug("after: 0x%x\n", *ptr_in_available_mem);
   // ca marche
   int *ptr_in_reserved_mem = (int *)0xf0000; // Adresse en ROM BIOS (réservée)
   debug("Reserved mem (0xf0000): before: 0x%x ", *ptr_in_reserved_mem);
   *ptr_in_reserved_mem = 0xaaaaaaaa;
   debug("after: 0x%x\n", *ptr_in_reserved_mem);
   // rien n'a été écrit (fail silencieux)

   /* Q4 : test hors mémoire physique */
   int *ptr_out_of_bounds = (int *)0x10000000;
   debug("Out of bounds (0x10000000): before: 0x%x ", *ptr_out_of_bounds);
   *ptr_out_of_bounds = 0xaaaaaaaa;
   debug("after: 0x%x\n", *ptr_out_of_bounds);
   // rien n'est écrit, Le bus mémoire non connecté (floating bus) renvoit 0.


}
