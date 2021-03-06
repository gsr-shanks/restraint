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

#include "dependency.h"
#include "errors.h"

typedef struct {
    GError *error;
    GMainLoop *loop;
    GString *output;
} RunData;

gboolean
dependency_io_cb (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    RunData *run_data = (RunData *) user_data;
    GError *error = NULL;

    gchar buf[131072];
    gsize bytes_read;

    if (condition & (G_IO_IN )) {
        switch (g_io_channel_read_chars(io, buf, 131072, &bytes_read, &error)) {
          case G_IO_STATUS_NORMAL:
            g_string_append_len (run_data->output, buf, bytes_read);
            return TRUE;

          case G_IO_STATUS_ERROR:
             g_clear_error (&error);
             return FALSE;

          case G_IO_STATUS_EOF:
             g_string_append_len (run_data->output, "finished!\n", 10);
             return FALSE;

          case G_IO_STATUS_AGAIN:
             g_string_append_len (run_data->output, "Not ready.. try again.\n", 24);
             return TRUE;

          default:
             g_return_val_if_reached(FALSE);
             break;
        }
    }

    if (condition & G_IO_HUP){
        return FALSE;
    }
    return FALSE;
}

void
dependency_finish_cb (gpointer user_data, GError *error)
{
    RunData *run_data = (RunData *) user_data;
    if (error)
        run_data->error = g_error_copy (error);
    g_main_loop_quit (run_data->loop);
    g_main_loop_unref (run_data->loop);
}

static void test_dependencies_success (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "PackageB");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "yum -y install PackageC\r\ndummy yum: installing PackageC\r\nyum -y install PackageB\r\ndummy yum: installing PackageB\r\nyum -y install PackageA\r\ndummy yum: installing PackageA\r\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    restraint_install_dependencies (dependencies,
                                    FALSE,
                                    dependency_io_cb,
                                    dependency_finish_cb,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);
}

static void test_dependencies_fail (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "Packagefail");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "yum -y install PackageC\r\ndummy yum: installing PackageC\r\nyum -y install Packagefail\r\ndummy yum: fail\r\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    restraint_install_dependencies (dependencies,
                                    FALSE,
                                    dependency_io_cb,
                                    dependency_finish_cb,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_error (run_data->error, RESTRAINT_ERROR, RESTRAINT_TASK_RUNNER_RC_ERROR);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);
}

static void test_dependencies_ignore_fail (void)
{
    RunData *run_data;
    GSList *dependencies = NULL;
    dependencies = g_slist_prepend (dependencies, "PackageA");
    dependencies = g_slist_prepend (dependencies, "Packagefail");
    dependencies = g_slist_prepend (dependencies, "PackageC");
    gchar *expected = "yum -y install PackageC\r\ndummy yum: installing PackageC\r\nyum -y install Packagefail\r\ndummy yum: fail\r\nyum -y install PackageA\r\ndummy yum: installing PackageA\r\n";

    run_data = g_slice_new0 (RunData);
    run_data->loop = g_main_loop_new (NULL, TRUE);
    run_data->output = g_string_new (NULL);

    restraint_install_dependencies (dependencies,
                                    TRUE,
                                    dependency_io_cb,
                                    dependency_finish_cb,
                                    run_data);

    // run event loop while process is running.
    g_main_loop_run (run_data->loop);

    // process finished, check our results.
    g_assert_no_error (run_data->error);
    g_clear_error (&run_data->error);
    g_assert_cmpstr(run_data->output->str, == , expected);
    g_string_free (run_data->output, TRUE);
    g_slice_free (RunData, run_data);
    g_slist_free (dependencies);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/dependencies/success", test_dependencies_success);
    g_test_add_func("/dependencies/failure", test_dependencies_fail);
    g_test_add_func("/dependencies/ignore_failure", test_dependencies_ignore_fail);
    return g_test_run();
}
