/**
 *   author       :   丁雪峰
 *   time         :   2014-09-01 10:07:24
 *   email        :   fengidri@yeah.net
 *   version      :   1.0.1
 *   description  :
 */
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>

#include <gtk/gtk.h>  
#include "quick_search.h"
#include "vuiint.h"
#include "rstr/sds.h"
#include "stdlib.h"

#include "gtklist.h"

#define HI_FMT  "<span color='red'>%c</span>"
#define true 1
#define false 0

static cJSON *infos;
static GtkWidget *window;
void list_clear(GtkWidget *list);
void list_append(GtkWidget *list, const gchar *str, const gchar *value);
void list_select(GtkWidget *list, int index);
int list_selected(GtkWidget *list, int column, GValue *buf);
static void list_info_init(GtkWidget *list, cJSON *json);

struct SortedItem{
    int sv;//sort value
    void *data;
    void *data1;
};

void qs_set_infos(cJSON *json)
{
    infos = json;
}

int diffuse( const char *patten, const char *string)
{
    int tmp;
    int index = 0;
    int last = 0;
    int ret = 0;
    while( *string )
    {
        tmp = *patten - *string;
        if ( tmp == 32 || tmp == 0 )
        {
            patten++;
            ret = index - last + ret;
            if (0 == *patten)
            {
                return  ret;
            }
        }
        string++;
        index ++;
    }
    return  -1;
}

bool himatch_diffuse(const char *patten, const char *string, sds *buf, int *m)
{
    int tmp;
    int index = 0;
    int last = 0;
    sds buffer;
    buffer = *buf;

    *m = 0;
    while( *string )
    {
        tmp = *patten - *string;
        if ( tmp == 32 || tmp == 0 )
        {
            patten++;
            *m = index - last + *m;
            last = index;

            buffer = sdscatprintf(buffer, HI_FMT , *string);
            if (0 == *patten)
                *buf = buffer;
                return  sdscat(buffer, string + 1);

        }
        else{
            buffer = sdscatlen(buffer, string, 1);
        }
        string++;
        index ++;
    }
    sdsfree(buffer);
    return  NULL;

}



const char * strstrcase(const char *src, const char *pat, size_t size)
{
    size_t offset;
    int tmp;
    const char *p;
    do{
        p = pat;
        offset = 0;
        do{
            tmp = *p - *(src + offset);
            if(0 != tmp && 32 != tmp) break;
            ++offset;
            ++p;
            if (offset >= size) return src;
        }while(1);

    }while(*src++);
    return NULL;
}

bool himatch_key(const char *patten, const char *string, sds *buf, int *m)
{
    sds buffer;
    int size;
    const char *start, *target, *pos;

    buffer = *buf;
    pos = patten;
    *m = 0;

    while(1){// 首先分割patten
        // 跳过空格
        while(' ' == *pos && 0 != *pos) pos ++;
        start = pos;
        if (0 == *start) {
            buffer = sdscat(buffer, string);
            *buf = buffer;
            return true;// key 全部取完了
        }
        // 跳过一般字符
        while (' ' != *pos && 0 != *pos) pos ++;

        size = pos - start;

        // 在string 中查找关键词
        target = strstrcase(string, start, size);
        if (NULL == target) {
            *buf = buffer;
            return false; // 没找到
        }

        *m += target - string;
        buffer = sdscatlen(buffer, string, target - string);
        buffer = sdscatprintf(buffer, "<span color='red'>%.*s</span>",
                size, target);
        string = target +  size;
    };

}

static void list_hi_show(GtkTreeViewColumn *col,
                    GtkCellRenderer   *renderer,
                    GtkTreeModel      *model,
                    GtkTreeIter       *iter,
                    gpointer           user_data)
{
  char *value;
  gtk_tree_model_get(model, iter, 0, &value, -1);
  g_object_set(renderer, "markup", value,  NULL);
}



static int getpatten(char *pat, int size, const char *text)
{
    int len;
    len = strlen(text);
    len  = len < size - 1 ? len :size -1;
    memcpy(pat, text, len);
    *(pat + len) = 0;
    return 0;
}

static inline int compar(const void *a, const void* b)
{
    return ((struct SortedItem*)a)->sv - ((struct SortedItem*)b)->sv;

}

static void *filter_patten(char *patten, cJSON *json, int *s)
{
    struct SortedItem *nf, *si;
    struct SortedItem **node;
    const char *filter, *pre,*value;
    unsigned int size, i, m;
    sds data = NULL;
    cJSON *item;
    size = cJSON_GetArraySize(json);

    *s = 0;
    si = nf = (struct SortedItem*)malloc(sizeof(struct SortedItem)*size);
    for (i = 0; i < size; ++i)
    {
        item  = cJSON_GetArrayItem(infos, i);

        pre   = cJSON_GetArrayItem(item, 0)->valuestring;
        filter = cJSON_GetArrayItem(item, 1)->valuestring;

        if (!data)  data = sdsnew(pre);
        else   data = sdscpy(data, pre);

        if (!himatch_key(patten, filter, &data, &si->sv)) continue;
        si->data = data;
        data = NULL;

        si->data1 = cJSON_GetArrayItem(item, 2)->valuestring;
        ++si;
        ++*s;
    }
    if(data) sdsfree(data);
    qsort(nf, *s, sizeof(struct SortedItem), compar);
    return nf;
}


static void entry_change(GtkWidget *entry, gpointer data){
    GtkWidget *list;
    int size;
    struct SortedItem *nf;
    void *nfree;
    static char *fv = NULL;//first value
    char  patten[100];

    list = (GtkWidget*)data;

    if (0 != getpatten(patten, sizeof(patten), 
                gtk_entry_get_text(GTK_ENTRY(entry))))
        return;
    if (0 == strlen(patten))
    {
        list_info_init(list, infos);
        return;
    }
    nfree = nf = filter_patten(patten, infos, &size);
    if (size && nf->data1 != fv)// 只在首选发生变化的时候通知
    {
        fv = nf->data1;
        set_code(110, NULL);
        send_replay(fv);
    }

    list_clear(list);
    while(size)
    {
        list_append(list, nf->data, nf->data1);
        sdsfree(nf->data);
        ++nf;
        --size;
    }
    free(nfree);
    list_select(list, 0);
}
static void entry_activate(GtkWidget *w, gpointer data){
    GtkWidget *list;
    GValue value={0,};
    list = (GtkWidget *)data;
    if (0 == list_selected(list, 1, &value))
    {
        send_replay((char*)g_value_get_string(&value));
        g_value_unset(&value);
    }
    gtk_widget_destroy(window);
    gtk_main_quit();
}

static void list_activated(GtkWidget *w, gpointer data){
    GValue value={0,};
    if (0 == list_selected(w, 1, &value))
    {
        send_replay((char*)g_value_get_string(&value));
        g_value_unset(&value);
    }
    gtk_widget_destroy(window);
    gtk_main_quit();
}

static void list_info_init(GtkWidget *list, cJSON *json)
{
    int i;
    cJSON *item;
    const char *value, *filter, *pre;
    char  showbuf[1024];
    list_clear(list);
    for(i=0; i< cJSON_GetArraySize(json) && i < 30;i++)
    {
        item  = cJSON_GetArrayItem(json, i);
        pre = cJSON_GetArrayItem(item,0)->valuestring;
        value = cJSON_GetArrayItem(item,2)->valuestring;
        filter = cJSON_GetArrayItem(item, 1)->valuestring;
        snprintf(showbuf, sizeof(showbuf), "%s%s", pre, filter);

        list_append(list, showbuf, value);
    }
    list_select(list, 0);

}
static gboolean list_keys(GtkWidget *w,  GdkEventKey *event, gpointer data){
    if (event->keyval == GDK_KEY_j)
    {
        list_selnext(w);
        return TRUE;

    }
    else if (event->keyval == GDK_KEY_k)
    {
        list_selpre(w);
        return TRUE;

    }

        return FALSE;
}


static gboolean check_escape(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  if (event->keyval == GDK_KEY_Escape) {
    gtk_widget_destroy(widget);
    gtk_main_quit();
    return TRUE;
  }
  return FALSE;
}

GtkWidget * window_init()
{
    gint sw, sh;
    gint ww, wh;
    GdkScreen *screen;
    GtkWidget *w; 

    w = gtk_window_new(GTK_WINDOW_TOPLEVEL);  
    gtk_window_set_keep_above(GTK_WINDOW(w), TRUE);


    gtk_window_set_title(GTK_WINDOW(w), "VuiRpcQs");  
    gtk_window_set_default_size(GTK_WINDOW(w), 500, 550);

    screen = gdk_screen_get_default();
    sw = gdk_screen_get_width(screen);
    sh = gdk_screen_get_height(screen);
    gtk_window_get_size(GTK_WINDOW(w), &ww, &wh);
    gtk_window_move(GTK_WINDOW(w), (sw - ww)/2, 150);

    gtk_container_set_border_width(GTK_CONTAINER(w), 10);  
    g_signal_connect(GTK_WINDOW(w), "destroy", G_CALLBACK(gtk_main_quit), NULL);  
    g_signal_connect(GTK_WINDOW(w), "key_press_event", G_CALLBACK(check_escape), NULL);
    return w;
}

GtkWidget *entry_init(GtkWidget *list)
{
    GtkWidget *wentry;  
    PangoFontDescription *font;
    wentry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(wentry), "");
    g_signal_connect(GTK_ENTRY(wentry), "changed", G_CALLBACK(entry_change), list);
    g_signal_connect(GTK_ENTRY(wentry), "activate", G_CALLBACK(entry_activate), list);
    font = pango_font_description_from_string("monaco 18");
    gtk_widget_override_font(wentry, font);

    return  wentry;
}

GtkWidget * list_init()
{
    GtkTreeIter iter;
    GtkListStore *store;
    GtkWidget *tree;
    GtkTreeViewColumn *column;
    PangoFontDescription *font;

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "text", renderer, 
            "text", 0,NULL);

    gtk_tree_view_column_set_cell_data_func(column, renderer, list_hi_show, NULL, NULL);
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    gtk_tree_view_append_column(GTK_TREE_VIEW (tree), column);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
    g_signal_connect(tree, "row-activated", G_CALLBACK(list_activated), NULL);
    g_signal_connect(tree, "key-press-event", G_CALLBACK(list_keys), NULL);

    font = pango_font_description_from_string("monaco 12");
    gtk_widget_override_font(tree, font);
    return tree;
}



  
void quick_search()  
{  
    gtk_init(NULL, NULL);  
    GtkWidget *grid, *entry, *list, *table, *scroll;
  
    window = window_init();
    list = list_init();
    entry = entry_init(list);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), list);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    grid = gtk_grid_new();  
    gtk_container_add(GTK_CONTAINER(window), grid);  
    gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), scroll, 0, 1, 1, 1);

    list_info_init(list, infos);

    gtk_widget_show_all(window);  
    gtk_main();  
}  