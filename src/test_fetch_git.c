/*  
    This file is part of Restraint.

    Restraint is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Restraint is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Restraint.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <libsoup/soup-uri.h>
#include <archive.h>

#include "fetch.h"
#include "fetch_git.h"

typedef struct {
    GString *entry;
    GError *error;
    GMainLoop *loop;
} RunData;

static void
archive_entry_callback (const gchar *entry, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    run_data->entry = g_string_append (run_data->entry, entry);
}

static void
fetch_finish_callback (GError *error, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    if (error)
        g_propagate_error (&run_data->error, error);
    g_main_loop_quit (run_data->loop);
    g_main_loop_unref (run_data->loop);
}

static void
test_fetch_git_success(void) {
    RunData *run_data;

    gchar *expected = "MakefilePURPOSEmetadataruntest.sh";
    run_data = g_slice_new0 (RunData);
    run_data->entry = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    SoupURI *url = soup_uri_new ("git://localhost/repo1?master#restraint/sanity/fetch_git");
    gchar *path = g_dir_make_tmp ("test_fetch_git_XXXXXX", NULL);

    restraint_fetch_git (url,
                         path,
                         archive_entry_callback,
                         fetch_finish_callback,
                         run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // check that initial request worked.
    g_assert_no_error (run_data->error);

    // check that output is expected
    g_assert_cmpstr (run_data->entry->str, ==, expected);

    GFile *base = g_file_new_for_path (path);

    GFile *file = g_file_get_child (base, "Makefile");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "PURPOSE");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "metadata");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    file = g_file_get_child(base, "runtest.sh");
    g_assert(g_file_query_exists (file, NULL) != FALSE);
    g_file_delete (file, NULL, NULL);
    g_object_unref(file);

    g_object_unref(base);

    // free our memory
    g_string_free (run_data->entry, TRUE);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
    g_remove (path);
    g_free (path);
    soup_uri_free (url);
}

static void
test_fetch_git_fail(void) {
    RunData *run_data;

    gchar *expected = "";
    run_data = g_slice_new0 (RunData);
    run_data->entry = g_string_new (NULL);
    run_data->loop = g_main_loop_new (NULL, TRUE);

    SoupURI *url = soup_uri_new ("git://localhost/repo1?master#restraint/sanity/fetch_gt");
    gchar *path = g_dir_make_tmp ("test_fetch_git_XXXXXX", NULL);

    restraint_fetch_git (url,
                         path,
                         archive_entry_callback,
                         fetch_finish_callback,
                         run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // check that initial request worked.
    g_assert_error (run_data->error, RESTRAINT_FETCH_LIBARCHIVE_ERROR, ARCHIVE_FATAL);

    // check that output is expected
    g_assert_cmpstr (run_data->entry->str, ==, expected);

    // free our memory
    g_string_free (run_data->entry, TRUE);
    g_clear_error (&run_data->error);
    g_slice_free (RunData, run_data);
    g_remove (path);
    g_free (path);
    soup_uri_free (url);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/fetch_git/success", test_fetch_git_success);
    g_test_add_func("/fetch_git/fail", test_fetch_git_fail);
    return g_test_run();
}
