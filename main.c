
#include <gtk/gtk.h>
GtkTextBuffer *buffer;

static guint timer_id = 0;

static gboolean onStoppedTyping() {
  g_message("Stopped typing!");
  timer_id = 0;
  return G_SOURCE_REMOVE;
}

static void on_button_clicked(GtkButton *button, gpointer user_data) {
  g_message("Save button clicked!");
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  gchar *text;
  text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  if (text) {
    g_message("%s", text);
    g_free(text);
  }
}

static void redoButtonHandler(GtkButton *button, gpointer user_data) {
  g_message("Redo button clicked!");
}

static void undoButtonHandler(GtkButton *button, gpointer user_data) {
  g_message("Undo button clicked!");
}

static void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
  g_message("Text changed!");
  if (timer_id) {
    g_source_remove(timer_id);
  }
  timer_id = g_timeout_add(500, onStoppedTyping, NULL);
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *header;
  GtkWidget *save_button;
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
  GtkWidget *redoButton;
  GtkWidget *undoButton;

  // Create a new window
  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Hayat Editor");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  // Create a vertical box layout (container)
  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(window), box);

  // Create a header bar using a horizontal box
  header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_size_request(header, -1, 50);   // Set height
  gtk_widget_set_halign(header, GTK_ALIGN_FILL); // Full width

  // Create a save button
  save_button = gtk_button_new_with_label("Save");
  g_signal_connect(save_button, "clicked", G_CALLBACK(on_button_clicked), NULL);

  // undo redo button
  redoButton = gtk_button_new_with_label("Redo");
  g_signal_connect(redoButton, "clicked", G_CALLBACK(redoButtonHandler), NULL);

  undoButton = gtk_button_new_with_label("Undo");
  g_signal_connect(undoButton, "clicked", G_CALLBACK(undoButtonHandler), NULL);

  // Add button to the header
  gtk_box_append(GTK_BOX(header), save_button);
  gtk_box_append(GTK_BOX(header), undoButton);
  gtk_box_append(GTK_BOX(header), redoButton);

  // Create a scrolled text area
  scrolled_window = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(scrolled_window, TRUE);

  text_view = gtk_text_view_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
                                text_view);
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  gtk_text_buffer_set_text(buffer, "Start typing...", -1);
  g_signal_connect(buffer, "changed", G_CALLBACK(on_text_changed), NULL);

  // Add header and text area to the main box
  gtk_box_append(GTK_BOX(box), header);
  gtk_box_append(GTK_BOX(box), scrolled_window);

  // Show the window
  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.editor", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
