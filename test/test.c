#include <gtk/gtk.h>

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkCssProvider *provider;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, "window { background-color: rgb(0,255,0); }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                 GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);

  gtk_window_move (GTK_WINDOW (window), 100, 100);

  gtk_main ();

  return 0;
}
