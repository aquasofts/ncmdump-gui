#include <gtk/gtk.h>
#include <windows.h>
#include <shlobj.h>
#include <gdk/gdkwin32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <io.h>

void load_css();
void create_ncmmp3_folder();
void execute_command(GtkWindow *parent, const char *command_fmt, ...);
gboolean show_confirmation_dialog(GtkWindow *parent, const char *title, const char *message);
char* select_single_file(HWND hwnd);
char* select_folder(HWND hwnd);
HWND get_hwnd_from_gtk_window(GtkWindow *window);
void on_file_selected(GtkWidget *widget, gpointer data);
void on_folder_selected(GtkWidget *widget, gpointer data);
void set_background(GtkWindow *window);

wchar_t* utf8_to_wide(const char* utf8_str) {
    if (!utf8_str) return NULL;
    int wchars_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    wchar_t* wide_str = (wchar_t*)malloc(wchars_needed * sizeof(wchar_t));
    if (wide_str) {
        MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str, wchars_needed);
    }
    return wide_str;
}

char* wide_to_utf8(const wchar_t* wide_str) {
    if (!wide_str) return NULL;
    int utf8_chars_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
    char* utf8_str = (char*)malloc(utf8_chars_needed);
    if (utf8_str) {
        WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, utf8_str, utf8_chars_needed, NULL, NULL);
    }
    return utf8_str;
}

void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    GError *error = NULL;
    if (!gtk_css_provider_load_from_file(provider, g_file_new_for_path("style.css"), &error)) {
        GtkWidget *dialog = gtk_message_dialog_new(
            NULL,
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "CSS加载失败: %s", error->message
        );
        gtk_window_set_title(GTK_WINDOW(dialog), "错误");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }

    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

void create_ncmmp3_folder() {
    struct stat st = {0};
    if (stat("ncmmp3", &st) == -1) {
        if (!CreateDirectoryA("ncmmp3", NULL)) {
            GtkWidget *dialog = gtk_message_dialog_new(
                NULL,
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "无法创建ncmmp3文件夹"
            );
            gtk_window_set_title(GTK_WINDOW(dialog), "错误");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
    }
}

void execute_command(GtkWindow *parent, const char *command_fmt, ...) {
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char ncmdump_args_buffer[4096];
    va_list args;
    va_start(args, command_fmt);
    vsnprintf(ncmdump_args_buffer, sizeof(ncmdump_args_buffer), command_fmt, args);
    va_end(args);

    char final_cmd_line[sizeof(ncmdump_args_buffer) + 200];
    snprintf(final_cmd_line, sizeof(final_cmd_line),
             "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"& { .\\ncmdump %s }\"",
             ncmdump_args_buffer);

    wchar_t* wide_final_cmd_line = utf8_to_wide(final_cmd_line);
    if (!wide_final_cmd_line) {
        GtkWidget *dialog = gtk_message_dialog_new(
            parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "命令编码失败！"
        );
        gtk_window_set_title(GTK_WINDOW(dialog), "错误");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    FILE *debug_log = fopen("debug_log.txt", "a");
    if (debug_log) {
        fwprintf(debug_log, L"Full command to CreateProcessW: %s\n", wide_final_cmd_line);
        fclose(debug_log);
    }

    if (!CreateProcessW(
            NULL,
            wide_final_cmd_line,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            (LPSTARTUPINFOW)&si,
            &pi
        )) {
        DWORD error = GetLastError();
        char errorMsg[512];
        g_snprintf(errorMsg, sizeof(errorMsg), "执行命令失败 (错误码: %lu)。请确保powershell.exe和ncmdump.exe在PATH中或本程序目录。", error);

        GtkWidget *dialog = gtk_message_dialog_new(
            parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "%s", errorMsg
        );
        gtk_window_set_title(GTK_WINDOW(dialog), "错误");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        free(wide_final_cmd_line);
        return;
    }

    free(wide_final_cmd_line);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        if (exitCode != 0) {
            char msg[512];
            g_snprintf(msg, sizeof(msg), "命令执行失败，返回代码: %lu。这可能意味着文件损坏或ncmdump错误。", exitCode);

            GtkWidget *dialog = gtk_message_dialog_new(
                parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                "%s", msg
            );
            gtk_window_set_title(GTK_WINDOW(dialog), "执行结果");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        } else {
            GtkWidget *dialog = gtk_message_dialog_new(
                parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                "转换完成！已保存到ncmmp3目录"
            );
            gtk_window_set_title(GTK_WINDOW(dialog), "成功");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

gboolean show_confirmation_dialog(GtkWindow *parent, const char *title, const char *message) {
    GtkWidget *dialog;
    gint response;

    dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s", message
    );

    gtk_window_set_title(GTK_WINDOW(dialog), title);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return (response == GTK_RESPONSE_YES);
}

char* select_single_file(HWND hwnd) {
    OPENFILENAMEW ofn;
    static wchar_t szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"NCM文件\0*.ncm\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        char* result = wide_to_utf8(szFile);
        return result;
    }

    return NULL;
}

char* select_folder(HWND hwnd) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"选择文件夹";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        wchar_t wide_path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, wide_path)) {
            CoTaskMemFree(pidl);
            char* path = wide_to_utf8(wide_path);
            return path;
        }
    }

    return NULL;
}

HWND get_hwnd_from_gtk_window(GtkWindow *window) {
    GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
    return gdk_win32_window_get_handle(gdk_window);
}

void on_file_selected(GtkWidget *widget, gpointer data) {
    GtkWindow *window = GTK_WINDOW(data);
    HWND hwnd = get_hwnd_from_gtk_window(window);

    char* filename = select_single_file(hwnd);
    if (filename) {
        char message[1024];
        g_snprintf(message, sizeof(message), "确定要转换此文件吗？（可能会出现玄学问题，如无输出结果可使用文件夹转换功能）\n\n文件: %s", filename);

        if (show_confirmation_dialog(window, "确认转换", message)) {
            execute_command(window, "\"%s\" -o ncmmp3", filename);
        }

        free(filename);
    }
}

void on_folder_selected(GtkWidget *widget, gpointer data) {
    GtkWindow *window = GTK_WINDOW(data);
    HWND hwnd = get_hwnd_from_gtk_window(window);

    char* foldername = select_folder(hwnd);
    if (foldername) {
        char message[1024];
        g_snprintf(message, sizeof(message), "确定要转换此文件夹中的所有文件吗？\n\n文件夹: %s", foldername);

        if (show_confirmation_dialog(window, "确认转换", message)) {
            execute_command(window, "-d \"%s\" -o ncmmp3", foldername);
        }

        free(foldername);
    }
}

void set_background(GtkWindow *window) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file("./pic/bg.jpg", &error);

    if (!pixbuf) {
        g_printerr("背景图片加载失败: %s\n", error->message);
        GtkWidget *error_label = gtk_label_new("背景图片加载失败！\n请检查pic/bg.jpg是否存在");
        gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
        gtk_container_add(GTK_CONTAINER(window), error_label);

        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider, "window { background-color: red; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(window)),
                                      GTK_STYLE_PROVIDER(provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);

        g_error_free(error);
        return;
    }

    GtkWidget *background = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(window), overlay);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), background);

    gtk_widget_set_events(background, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t exe_path_w[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path_w, MAX_PATH);

    wchar_t* last_slash = wcsrchr(exe_path_w, L'\\');
    if (last_slash != NULL) {
        *last_slash = L'\0';
    }

    if (!SetCurrentDirectoryW(exe_path_w)) {
        MessageBoxW(NULL, L"无法设置当前工作目录。", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    CoInitialize(NULL);
    gtk_init(NULL, NULL);

    load_css();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "NCM转MP3工具");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GError *error = NULL;
    GdkPixbuf *icon = gdk_pixbuf_new_from_file("app.ico", &error);
    if (icon) {
        gtk_window_set_icon(GTK_WINDOW(window), icon);
        g_object_unref(icon);
    } else if (error) {
        g_error_free(error);
    }

    set_background(GTK_WINDOW(window));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 50);

    GtkWidget *overlay = gtk_bin_get_child(GTK_BIN(window));
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), main_box);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);

    GtkWidget *title_label = gtk_label_new("<span size='xx-large' weight='bold'>NCM转MP3工具</span>");
    gtk_label_set_use_markup(GTK_LABEL(title_label), TRUE);
    gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 30);

    GtkWidget *file_button = gtk_button_new_with_label("选择单个文件");
    gtk_widget_set_size_request(file_button, 200, 50);
    g_signal_connect(file_button, "clicked", G_CALLBACK(on_file_selected), window);
    gtk_box_pack_start(GTK_BOX(main_box), file_button, FALSE, FALSE, 10);

    GtkWidget *folder_button = gtk_button_new_with_label("选择文件夹");
    gtk_widget_set_size_request(folder_button, 200, 50);
    g_signal_connect(folder_button, "clicked", G_CALLBACK(on_folder_selected), window);
    gtk_box_pack_start(GTK_BOX(main_box), folder_button, FALSE, FALSE, 10);

    create_ncmmp3_folder();

    gtk_widget_show_all(window);
    gtk_main();

    CoUninitialize();
    return 0;
}