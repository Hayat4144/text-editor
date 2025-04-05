#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

GtkTextBuffer *buffer;
#define MAX_STACK_SIZE 50

typedef struct StackNode {
  char *text;
  struct StackNode *next;
} StackNode;

StackNode *undo_stack = NULL;
StackNode *redo_stack = NULL;
static guint typing_timer = 0;

// Stack operations
char *stack_pop(StackNode **stack) {
  if (*stack == NULL)
    return NULL;

  StackNode *top = *stack;
  char *text = strdup(top->text); // Create a copy
  *stack = top->next;
  free(top->text);
  free(top);
  return text;
}

void stack_clear(StackNode **stack) {
  while (*stack) {
    StackNode *top = *stack;
    *stack = top->next;
    free(top->text);
    free(top);
  }
}

static void stack_push(StackNode **stack, const char *text) {
  // Count current stack size
  int count = 0;
  StackNode *current = *stack;
  while (current != NULL) {
    count++;
    current = current->next;
  }

  // Remove oldest node if stack is full
  if (count >= MAX_STACK_SIZE) {
    StackNode *prev = NULL;
    StackNode *last = *stack;

    while (last->next != NULL) {
      prev = last;
      last = last->next;
    }

    if (prev == NULL) {
      // Only one node in stack
      free((*stack)->text);
      free(*stack);
      *stack = NULL;
    } else {
      prev->next = NULL;
      free(last->text);
      free(last);
    }
  }

  // Add new node
  StackNode *new_node = malloc(sizeof(StackNode));
  if (new_node == NULL)
    return;

  new_node->text = strdup(text);
  if (new_node->text == NULL) {
    free(new_node);
    return;
  }

  new_node->next = *stack;
  *stack = new_node;
}

// Editor functions
static void capture_state() {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  if (text[0] != '\0' &&
      (undo_stack == NULL || strcmp(text, undo_stack->text) != 0)) {
    stack_push(&undo_stack, text);
    stack_clear(&redo_stack);
  }

  g_free(text);
}

static gboolean typing_stopped(gpointer user_data) {
  capture_state();
  typing_timer = 0;
  return G_SOURCE_REMOVE;
}

static void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
  if (typing_timer)
    g_source_remove(typing_timer);
  typing_timer = g_timeout_add(500, typing_stopped, NULL);
}

void undo_action(GtkButton *button, gpointer user_data) {
  if (undo_stack == NULL)
    return;

  // Get current state text
  char *current_text = strdup(undo_stack->text);
  if (!current_text)
    return;

  // Remove from undo stack
  StackNode *top = undo_stack;
  undo_stack = top->next;
  free(top->text);
  free(top);

  // Apply previous state (or empty if no more undo states)
  char *prev_text = undo_stack ? strdup(undo_stack->text) : strdup("");
  if (prev_text) {
    gtk_text_buffer_set_text(buffer, prev_text, -1);
    free(prev_text);
  }

  // Add to redo stack
  StackNode *new_node = malloc(sizeof(StackNode));
  if (new_node) {
    new_node->text = current_text;
    new_node->next = redo_stack;
    redo_stack = new_node;
  } else {
    free(current_text);
  }
}

void redo_action(GtkButton *button, gpointer user_data) {
  if (redo_stack == NULL)
    return;

  // Get redo state text
  char *redo_text = strdup(redo_stack->text);
  if (!redo_text)
    return;

  // Remove from redo stack
  StackNode *top = redo_stack;
  redo_stack = top->next;
  free(top->text);
  free(top);

  // Apply the redone state
  gtk_text_buffer_set_text(buffer, redo_text, -1);

  // Add to undo stack
  StackNode *new_node = malloc(sizeof(StackNode));
  if (new_node) {
    new_node->text = redo_text;
    new_node->next = undo_stack;
    undo_stack = new_node;
  } else {
    free(redo_text);
  }
}

static void save_dialog_callback(GObject *source, GAsyncResult *result,
                                 gpointer user_data) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
  GFile *file = gtk_file_dialog_save_finish(dialog, result, NULL);

  if (file) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gchar *path = g_file_get_path(file);

    if (!g_file_set_contents(path, text, -1, NULL)) {
      // Error handling
    }

    g_free(text);
    g_free(path);
    g_object_unref(file);
  }
  g_object_unref(dialog);
}

void save_action(GtkButton *button, gpointer user_data) {
  // Get the root window (proper GTK4 way)
  GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));
  if (!root) {
    g_warning("Button is not in a widget hierarchy with a root window");
    return;
  }

  GtkWindow *window = GTK_WINDOW(root); // Cast to GtkWindow  GtkFileDialog
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Save File");
  gtk_file_dialog_set_initial_name(dialog, "untitled.txt");

  gtk_file_dialog_save(dialog, GTK_WINDOW(window), NULL,
                       (GAsyncReadyCallback)save_dialog_callback, NULL);

  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  g_message("text %s", text);
  free(text);
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Text Editor");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  // Create main vertical box - this will contain toolbar and scrolled window
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(window), box);

  // Create toolbar (won't expand vertically)
  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *save_btn = gtk_button_new_with_label("Save");
  GtkWidget *undo_btn = gtk_button_new_with_label("Undo");
  GtkWidget *redo_btn = gtk_button_new_with_label("Redo");

  g_signal_connect(undo_btn, "clicked", G_CALLBACK(undo_action), NULL);
  g_signal_connect(redo_btn, "clicked", G_CALLBACK(redo_action), NULL);
  g_signal_connect(save_btn, "clicked", G_CALLBACK(save_action), NULL);
  gtk_box_append(GTK_BOX(toolbar), save_btn);
  gtk_box_append(GTK_BOX(toolbar), undo_btn);
  gtk_box_append(GTK_BOX(toolbar), redo_btn);
  gtk_box_append(GTK_BOX(box), toolbar);

  // Create scrolled window for text view (will expand to fill remaining space)
  GtkWidget *scrolled = gtk_scrolled_window_new();

  // These are the critical lines that make it take full height:
  gtk_widget_set_vexpand(scrolled, TRUE); // Expand vertically
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  // Create and configure text view
  GtkWidget *text_view = gtk_text_view_new();
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
  gtk_text_buffer_set_text(buffer, "", -1);
  g_signal_connect(buffer, "changed", G_CALLBACK(on_text_changed), NULL);

  // Add text view to scrolled window
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

  // Add scrolled window to main box (will take remaining space)
  gtk_box_append(GTK_BOX(box), scrolled);

  gtk_window_present(GTK_WINDOW(window));
}
int main(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("com.example.editor", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  // Cleanup
  stack_clear(&undo_stack);
  stack_clear(&redo_stack);

  return status;
}
