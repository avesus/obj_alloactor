#ifndef RECOMPUTE_SYS_INFO_VALUES  // flag so that can recompute with make flag
#if __has_include("PRECOMPUTED_SYS_INFO.h")  // have the values
#include "PRECOMPUTED_SYS_INFO.h"            // will include _SYS_INFO_H_
#endif
#endif


#ifndef _SYS_INFO_H_  // this will only pass if PRECOMPUTED_SYS_INFO.h does not
                      // exist
#define _SYS_INFO_H_

#include <misc/error_handling.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


namespace sysi {

void create_defs(const char * outfile);
void find_precomputed_header_file(char * path);
void read_proc_cpu(uint32_t * phys_core_info,
                   uint32_t * vm_bit_info,
                   uint32_t * phys_m_bit_info);


#ifndef NPROCS
#define NPROCS sysconf(_SC_NPROCESSORS_ONLN)
#endif

#ifndef NCORES
#define NCORES sysi::compute_ncores()

uint32_t
compute_ncores() {
    uint32_t ncores;
    read_proc_cpu(&ncores, NULL, NULL);
    return ncores;
}

#endif

#ifndef PHYS_CORE
#define PHYS_CORE(X) sysi::compute_phys_core(X)

uint32_t
compute_phys_core(uint32_t logical_core_num) {
    uint32_t ret;
    char     buf[8] = { 0 };
    char *   end    = buf;

    char core_info_file[128];
    sprintf(core_info_file,
            "/sys/devices/system/cpu/cpu%d/topology/core_id",
            logical_core_num);
    FILE * fp = fopen(core_info_file, "r");
    ERROR_ASSERT(fp != NULL,
                 "Error unable to open core info file:\n\"%s\"\n",
                 core_info_file);

    ERROR_ASSERT(fgets(buf, 7, fp) != NULL,
                 "Error unable to read core info from file:\n\"%s\"\n",
                 core_info_file);

    ret = (uint32_t)strtol(buf, &end, 10);
    DIE_ASSERT(
        (end != buf),
        "Error unable get integer type content from file contents:\n\"%s\"\n",
        buf);
    fclose(fp);

    return ret;
}
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

#ifndef VM_NBITS
#define VM_NBITS sysi::compute_vm_nbits()


uint32_t
compute_vm_nbits() {
    uint32_t vm_nbits;
    read_proc_cpu(NULL, &vm_nbits, NULL);
    return vm_nbits;
}

#endif

#ifndef PHYS_M_NBITS
#define PHYS_M_NBITS sysi::compute_phys_m_nbits()

uint32_t
compute_phys_m_nbits() {
    uint32_t phys_m_nbits;
    read_proc_cpu(NULL, NULL, &phys_m_nbits);
    return phys_m_nbits;
}

#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE sysi::compute_cache_line_size()


uint32_t
compute_cache_line_size() {
    uint32_t ret;
    char     buf[8] = { 0 };
    char *   end    = buf;

    const char * cache_line_info_file =
        "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size";
    FILE * fp = fopen(cache_line_info_file, "r");
    ERROR_ASSERT(fp != NULL,
                 "Error unable to open cache line file:\n\"%s\"\n",
                 cache_line_info_file);

    ERROR_ASSERT(fgets(buf, 7, fp) != NULL,
                 "Error unable to read cache line size from file:\n\"%s\"\n",
                 cache_line_info_file);

    ret = (uint32_t)strtol(buf, &end, 10);
    DIE_ASSERT(
        (end != buf),
        "Error unable get integer type content from file contents:\n\"%s\"\n",
        buf);
    fclose(fp);

    return ret;
}


#endif


void
read_proc_cpu(uint32_t * phys_core_info,
              uint32_t * vm_bit_info,
              uint32_t * phys_m_bit_info) {
    uint32_t expec_reads =
        (!!vm_bit_info) + (!!phys_m_bit_info) + (!!phys_core_info);
    if (!expec_reads) {
        return;
    }

    const uint32_t buf_len      = 64;
    char           buf[buf_len] = { 0 };
    FILE *         fp           = fopen("/proc/cpuinfo", "r");
    ERROR_ASSERT(fp != NULL, "Error unable to open \"proc/cpuinfo\"\n");

    while (fgets(buf, buf_len, fp)) {
        // skip longer lines basically
        if ((vm_bit_info || phys_m_bit_info) &&
            (!strncmp(buf, "address sizes", strlen("address sizes")))) {
            // line == "address sizes : <INT> bits physical, <INT> bits
            // virtual
            char     waste[16];
            uint32_t _vm_bit_info, _phys_m_bit_info;
            if (sscanf(buf,
                       "%s %s %c %d %s %s %d %s %s",
                       waste,
                       waste,
                       waste,
                       &_phys_m_bit_info,
                       waste,
                       waste,
                       &_vm_bit_info,
                       waste,
                       waste) == 9) {
                if (phys_m_bit_info) {
                    *phys_m_bit_info = _phys_m_bit_info;
                    --expec_reads;
                }
                if (vm_bit_info) {
                    *vm_bit_info = _vm_bit_info;
                    --expec_reads;
                }
                if (!expec_reads) {
                    fclose(fp);
                    return;
                }
            }
        }
        else if (phys_core_info &&
                 (!strncmp(buf, "cpu cores", strlen("cpu cores")))) {
            char waste[16];
            if (sscanf(buf,
                       "%s %s %c %d",
                       waste,
                       waste,
                       waste,
                       phys_core_info) == 4) {
                --expec_reads;
                if (!expec_reads) {
                    fclose(fp);
                    return;
                }
            }
        }
    }
    DIE("Error unable to find all info from proc/cpuinfo\n");
}


void
find_precomputed_header_file(char * path) {
    DIR * d = NULL;
    char  cur_dir[128];
    char  to_project_lib[8] = "";
    ERROR_ASSERT(getcwd(cur_dir, 128) != NULL,
                 "Error getting current working directory\n");
    d = opendir("build");
    if (d == NULL) {
        strcpy(to_project_lib, "../lib");
    }
    else {
        strcpy(to_project_lib, "lib");
    }
    d = opendir(to_project_lib);
    DIE_ASSERT(d != NULL,
               "Error unable to find lib directory\n\tTested path: \"%s\"\n",
               to_project_lib);
    closedir(d);

    sprintf(path,
            "%s/%s/system/PRECOMPUTED_SYS_INFO.h",
            cur_dir,
            to_project_lib);
}


void
create_defs(const char * outfile) {
    FILE * fp = NULL;
    if (outfile == NULL || (!strcmp(outfile, ""))) {
        char default_path[128];
        find_precomputed_header_file(default_path);
        fp = fopen(default_path, "w+");
        ERROR_ASSERT(fp != NULL,
                     "Error unable to open file at:\n\"%s\"\n",
                     default_path);
    }
    else {
        fp = fopen(outfile, "w+");
        ERROR_ASSERT(fp != NULL,
                     "Error unable to open file at:\n\"%s\"\n",
                     outfile);
    }


    fprintf(fp, "#ifndef _SYS_INFO_H_\n");
    fprintf(fp, "#define _SYS_INFO_H_\n");
    fprintf(fp, "#define HAVE_CONST_SYS_INFO\n");
    fprintf(fp, "\n");
    fprintf(fp,
            "// Number of processors (this is cores includes hyperthreads)\n");
    fprintf(fp, "#define NPROCS %d\n", (uint32_t)NPROCS);
    fprintf(fp, "\n");
    fprintf(fp, "// Number of physical cores\n");
    fprintf(fp, "#define NCORES %d\n", NCORES);
    fprintf(fp, "\n");

    fprintf(fp, "// Logical to physical core lookup\n");
    fprintf(fp, "#define PHYS_CORE(X) get_phys_core(X)\n");
    fprintf(fp,
            "uint32_t inline __attribute__((always_inline)) "
            "__attribute__((const))\n");
    fprintf(fp, "get_phys_core(const uint32_t logical_core_num) {\n");
    fprintf(fp,
            "\tstatic const constexpr uint32_t core_map[NPROCS] = { %d",
            PHYS_CORE(0));
    for (uint32_t i = 1; i < NPROCS; ++i) {
        fprintf(fp, ", %d", PHYS_CORE(i));
    }
    fprintf(fp, " };\n");
    fprintf(fp, "\treturn core_map[logical_core_num];\n");
    fprintf(fp, "}\n\n");


    fprintf(fp, "// Virtual memory page size\n");
    fprintf(fp, "#define PAGE_SIZE %d\n", (uint32_t)PAGE_SIZE);
    fprintf(fp, "\n");
    fprintf(fp, "// Virtual memory address space bits\n");
    fprintf(fp, "#define VM_NBITS %d\n", VM_NBITS);
    fprintf(fp, "\n");
    fprintf(fp, "// Physical memory address space bits\n");
    fprintf(fp, "#define PHYS_M_NBITS %d\n", PHYS_M_NBITS);
    fprintf(fp, "\n");
    fprintf(fp, "// cache line size (should be same for L1, L2, and L3)\n");
    fprintf(fp, "#define CACHE_LINE_SIZE %d\n", CACHE_LINE_SIZE);
    fprintf(fp, "\n");
    fprintf(fp, "#endif\n");
    fclose(fp);
}


}  // namespace sysi

#endif
