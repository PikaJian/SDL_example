/* A simple example of using SDL with GTk */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <gtk-3.0/gtk/gtk.h>
#include <gtk-3.0/gdk/gdkx.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
static void *CreateWindowX11(int w, int h);
static void DestroyWindowX11(void *window);
static void *native_window = NULL;
static Display *dpy;

SDL_Texture *texture = NULL;
SDL_Surface *screensurface = NULL;
SDL_Renderer *renderer = NULL;
SDL_Window *win = NULL;
SDL_Event event;
static int done = 1;
GtkWidget *sdl_socket;
GtkWidget *statusbar;
static bool full = FALSE;
static GtkWidget *gtkwindow = NULL;
void clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
   if (event->type == GDK_2BUTTON_PRESS) {
      printf("dclicked\n");
      if (!full) {
         gtk_window_fullscreen(GTK_WINDOW(gtkwindow));
         int w, h;
         
         SDL_GetRendererOutputSize(renderer, &w, &h);
      //SDL_DestroyRenderer(renderer);
      //SDL_DestroyTexture(texture);
      //SDL_CreateRenderer(sdl_window,-1,1);
      //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
      //                            SDL_TEXTUREACCESS_STREAMING,1000,700);
         printf("screen size (%d,%d)\n", w, h);
         full = TRUE;
      } else {

         gtk_window_unfullscreen(GTK_WINDOW(gtkwindow));
         int w, h;
         SDL_GetRendererOutputSize(renderer, &w, &h);
         printf("screen size (%d,%d)\n", w, h);
         full = FALSE;
      }
   }
}
static void *CreateWindowX11(int w, int h) {
   Window window = 0;

   dpy = XOpenDisplay(NULL);
   if (dpy) {
      window =
          XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, w, h, 0, 0, 0);
      printf("Window XID is %ld\n", window);
      XMapRaised(dpy, window);
      XSync(dpy, False);
   }
   return (void *)window;
}

static void DestroyWindowX11(void *window) {
   if (dpy) {
      XDestroyWindow(dpy, (Window)window);
      XCloseDisplay(dpy);
   }
}

void exit() {
   done = 0;
   printf("QUIT!!!!\n");
   //SDL_Quit();
}
void configure_event(GtkWindow *window, GdkEvent *event, gpointer data) 
{
   SDL_SetWindowSize(win,event->configure.width,event->configure.height);
}
GtkWidget *create_gtkwindow() {
   GtkWidget *box;
   sdl_socket = gtk_drawing_area_new();
   gtkwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_position(GTK_WINDOW(gtkwindow), GTK_WIN_POS_CENTER);
   gtk_window_set_default_size(GTK_WINDOW(gtkwindow), 1280, 720);
   gtk_window_set_title(GTK_WINDOW(gtkwindow), "Hello World");
   box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(gtkwindow), box);
   gtk_widget_set_size_request(sdl_socket, 800, 600);
   // statusbar = gtk_statusbar_new();
   gtk_box_pack_start(GTK_BOX(box), sdl_socket, TRUE, TRUE, 0);
   // gtk_box_pack_start(GTK_BOX(box), statusbar, FALSE, FALSE, 0);
   g_signal_connect_swapped(G_OBJECT(gtkwindow), "destroy", G_CALLBACK(exit),
                            NULL);
   g_signal_connect(G_OBJECT(gtkwindow), "configure_event",
                    G_CALLBACK(configure_event), 0);
   g_signal_connect(G_OBJECT(gtkwindow), "button-press-event",
                    G_CALLBACK(clicked), NULL);
   gtk_widget_show_all(gtkwindow);

   return gtkwindow;
}
int test_thread(void *data) {
   while (done) {

      while (gtk_events_pending()) {
         gtk_main_iteration_do(FALSE);
      }
      // printf("test\n");
      SDL_Rect rect;
      rect.x = 0;
      rect.y = 0;
      rect.w = 800;
      rect.h = 600;
      SDL_RenderClear(renderer);

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
      SDL_RenderCopy(renderer, texture, &rect,NULL);
      SDL_RenderPresent(renderer);
      // SDL_FillRect(screensurface, NULL,
      //             SDL_MapRGB(screensurface->format, 255, 0, 0));
      // SDL_UpdateWindowSurface(window);
      while (SDL_PollEvent(&event)) {
         switch (event.type) {
            case SDL_QUIT:
               printf("SDL Quit\n");
               done = 1;
               break;
            default:
               break;
         }
      }
   }
}
int main(int argc, char *argv[]) {
   int width, height;
   Uint8 *keys;
   gtk_init(&argc, &argv);

   /* Create a main window */
   if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
      return 1;
   }
   // void* xid = CreateWindowX11(1280,720);
   create_gtkwindow();
   /* Hack to get SDL to use GTK window */
   /*{
      char SDL_windowhack[32];
      sprintf(SDL_windowhack, "SDL_WINDOWID=%ld",
              (Window)xid);
      putenv(SDL_windowhack);
   }*/
   GdkWindow *tempw = gtk_widget_get_window(sdl_socket);

#ifdef SDL_VIDEO_DRIVER_WINDOWS
   printf("Microsoft Window system");
#endif
#ifdef SDL_VIDEO_DRIVER_X11
   printf("XWindow System!!\n");
#endif
   // printf("Now Video Driver:%s\n",SDL_GetCurrentVideoDriver());
   // printf("Window's XID is %ld\n",(Window)xid);
   win = SDL_CreateWindowFrom((void *)GDK_WINDOW_XID(tempw));
   SDL_SetWindowTitle(win, "SDL Native Window Test");
   if (win == NULL) {
      fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
      return 1;
   }
   // SDL_HideWindow(window);
   printf("init SDL Video!!\n");
   renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
   if (!renderer) {
      printf("Create Renderer failed!!\n");
      exit(0);
   }
   printf("SDL_Init Video end!!\n");
   // screensurface = SDL_GetWindowSurface(window);
   // if (!screensurface) {
   //   printf("Create Renderer failed!!\n");
   //   exit(0);
   //}
   // int pix_format = -1;

   // SDL_Surface* pictsurfacr =
   // SDL_CreateRGBSurface(0,width,height,32,0,0,0,0);
   // switch (screensurface->format->format)
   //{
   //   case SDL_PIXELFORMAT_RGB24:
   //      pix_format = 1;
   //      break;
   //   case SDL_PIXELFORMAT_RGB332:
   //      pix_format = 2;
   //      break;
   //   case SDL_PIXELFORMAT_RGB444:
   //      pix_format = 3;
   //      break;
   //   case SDL_PIXELFORMAT_RGB555:
   //      pix_format = 4;
   //      break;
   //   case SDL_PIXELFORMAT_RGB565:
   //      pix_format = 5;
   //      break;
   //   case SDL_PIXELFORMAT_RGB888:
   //      pix_format = 6;
   //      break;
   //   case SDL_PIXELFORMAT_RGBA4444:
   //      pix_format = 7;
   //      break;
   //   case SDL_PIXELFORMAT_RGBA5551:
   //      pix_format = 8;
   //      break;
   //   case SDL_PIXELFORMAT_RGBA8888:
   //      pix_format = 9;
   //      break;
   //   case SDL_PIXELFORMAT_RGBX8888:
   //      pix_format = 10;
   //      break;
   //   case SDL_PIXELFORMAT_BGR24:
   //      pix_format = 11;
   //      break;
   //   case SDL_PIXELFORMAT_IYUV:
   //      pix_format = 12;
   //      break;
   //   case SDL_PIXELFORMAT_YV12:
   //      pix_format = 13;
   //      break;
   //   case SDL_PIXELFORMAT_YUY2:
   //      pix_format = 14;
   //      break;
   //}
   // printf("screensurface format:%d\n",pix_format);
   texture = SDL_CreateTextureFromSurface(renderer,SDL_LoadBMP("test.bmp"));                                           
   //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
   //                            SDL_TEXTUREACCESS_STATIC, 800, 600);

    if(!texture)
   {
      printf("Create Texture failed!!\n");
   }
   printf("init SDL Video finished!!\n");
   SDL_Thread* threadID = SDL_CreateThread(test_thread, "test", NULL);
   int threadReturnValue;
   SDL_WaitThread(threadID,&threadReturnValue);
}
