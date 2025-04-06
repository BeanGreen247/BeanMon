#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <sys/statvfs.h>

// BeanMon - A Simple Linux System Monitor
// Created by BeanGreen247 (GitHub)
// License: WTFPL

#define VERSION "a0.1"
#define MAX_CPUS 32767

#define LINE_HEIGHT 20
#define ADDITIONAL_LINES 3
#define BASE_OFFSET 30
#define MIN_WIDTH 150

typedef struct {
    unsigned long long user, nice, system, idle;
} CPUStats;

void draw_text(Display *d, Window w, GC gc, int x, int y, const char *text, const char *color) {
    XColor xcolor;
    Colormap colormap = DefaultColormap(d, 0);
    XParseColor(d, colormap, color, &xcolor);
    XAllocColor(d, colormap, &xcolor);
    XSetForeground(d, gc, xcolor.pixel);
    XDrawString(d, w, gc, x, y, text, strlen(text));
}

int get_cpu_count() {
    int count = 0;
    FILE *fp = fopen("/proc/stat", "r");
    char line[256];
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "cpu", 3) == 0) count++;
        }
        fclose(fp);
    }
    return count - 1; // Exclude "cpu" (aggregated) line
}

void get_cpu_usage(CPUStats *cpus, int cpu_count) {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp) {
        char line[256];
        fgets(line, sizeof(line), fp); // Skip the first "cpu" line
        for (int i = 0; i < cpu_count; i++) {
            if (fgets(line, sizeof(line), fp)) {
                sscanf(line, "cpu%d %llu %llu %llu %llu", &i, &cpus[i].user, &cpus[i].nice, &cpus[i].system, &cpus[i].idle);
            }
        }
        fclose(fp);
    }
}

void get_memory_info(int *free_mem, int *cached_mem) {
    FILE *fp = fopen("/proc/meminfo", "r");
    char label[256];
    int value;
    if (fp) {
        while (fscanf(fp, "%s %d kB", label, &value) != EOF) {
            if (strcmp(label, "MemFree:") == 0)
                *free_mem = value / 1024;
            if (strcmp(label, "Cached:") == 0)
                *cached_mem = value / 1024;
        }
        fclose(fp);
    }
}

void get_disk_info(char *buffer) {
    struct statvfs fs;
    if (statvfs("/", &fs) == 0) {
        int free_space = (fs.f_bavail * fs.f_frsize) / (1024 * 1024 * 1024);
        snprintf(buffer, 256, "/ %dGiB", free_space);
    }
}

int main(int argc, char *argv[]) {
    Display *d = XOpenDisplay(NULL);
    if (!d) return 1;

    int screen = DefaultScreen(d);
    int cpu_count = get_cpu_count();

    // Layout calculations
    int dynamic_height = BASE_OFFSET + (cpu_count + ADDITIONAL_LINES) * LINE_HEIGHT;

    // Width estimate (longest possible string is probably "Core XX: 100%")
    int estimated_char_width = 8;  // rough average char width in pixels
    int max_text_length = strlen("Core 00: 100%") + 1;  // +1 for safety
    int WIDTH = estimated_char_width * max_text_length + 40;
    if (WIDTH < MIN_WIDTH) WIDTH = MIN_WIDTH;

    Window w = XCreateSimpleWindow(d, RootWindow(d, screen), 100, 100, WIDTH, dynamic_height, 1,
                                   BlackPixel(d, screen), BlackPixel(d, screen));

    XStoreName(d, w, "BeanMon");
    XSelectInput(d, w, ExposureMask | KeyPressMask);
    XMapWindow(d, w);

    XGCValues values;
    GC gc = XCreateGC(d, w, 0, &values);
    XSetForeground(d, gc, WhitePixel(d, screen));
    XSetBackground(d, gc, BlackPixel(d, screen));

    CPUStats prev[MAX_CPUS] = {0}, curr[MAX_CPUS];
    char cpu_text[256];
    char disk_info[256];
    int free_mem, cached_mem;

    while (1) {
        get_cpu_usage(curr, cpu_count);
        get_memory_info(&free_mem, &cached_mem);
        get_disk_info(disk_info);

        XClearWindow(d, w);
        for (int i = 0; i < cpu_count; i++) {
            unsigned long long prev_total = prev[i].user + prev[i].nice + prev[i].system + prev[i].idle;
            unsigned long long curr_total = curr[i].user + curr[i].nice + curr[i].system + curr[i].idle;
            unsigned long long total_diff = curr_total - prev_total;
            unsigned long long idle_diff = curr[i].idle - prev[i].idle;
            int usage = (total_diff > 0) ? (100 * (total_diff - idle_diff) / total_diff) : 0;

            snprintf(cpu_text, sizeof(cpu_text), "Core %d: %d%%", i, usage);
            draw_text(d, w, gc, 20, BASE_OFFSET + (i * LINE_HEIGHT), cpu_text, "cyan");
        }

        char mem_text[50];
        int text_y = BASE_OFFSET + (cpu_count * LINE_HEIGHT);
        snprintf(mem_text, 50, "Free RAM: %d MiB", free_mem);
        draw_text(d, w, gc, 20, text_y, mem_text, "yellow");

        snprintf(mem_text, 50, "Cached RAM: %d MiB", cached_mem);
        draw_text(d, w, gc, 20, text_y + LINE_HEIGHT, mem_text, "yellow");

        draw_text(d, w, gc, 20, text_y + 2 * LINE_HEIGHT, disk_info, "white");

        // Version display (top-right corner)
        char version_text[50];
        snprintf(version_text, sizeof(version_text), "v %s", VERSION);
        draw_text(d, w, gc, WIDTH - 60, 15, version_text, "gray");

        memcpy(prev, curr, sizeof(prev));
        XFlush(d);
        sleep(1.5f);
    }

    XCloseDisplay(d);
    return 0;
}
