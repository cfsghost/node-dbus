#include <stdlib.h>
#include <v8.h>
#include <node.h>
#include <string>
#include <dbus/dbus.h>

#include "connection.h"

namespace Connection {

	using namespace node;
	using namespace v8;
	using namespace std;

	static void watcher_handle(uv_poll_t *watcher, int status, int events)
	{
		DBusWatch *watch = (DBusWatch *)watcher->data;
		unsigned int flags = 0;

		if (events & UV_READABLE)
			flags |= DBUS_WATCH_READABLE;

		if (events & UV_WRITABLE)
			flags |= DBUS_WATCH_WRITABLE;

		dbus_watch_handle(watch, flags);
	}

	static void watcher_free(void *data)
	{
		uv_poll_t *watcher = (uv_poll_t *)data;

		watcher->data = NULL;
		free(watcher);
	}

	static dbus_bool_t watch_add(DBusWatch *watch, void *data)
	{
		if (!dbus_watch_get_enabled(watch) || dbus_watch_get_data(watch) != NULL)
			return true;

		int events = 0;
		int fd = dbus_watch_get_unix_fd(watch);
		unsigned int flags = dbus_watch_get_flags(watch);

		if (flags & DBUS_WATCH_READABLE)
			events |= UV_READABLE;

		if (flags & DBUS_WATCH_WRITABLE)
			events |= UV_WRITABLE;

		// Initializing watcher
		uv_poll_t *watcher = new uv_poll_t;
		watcher->data = (void *)watch;

		// Start watching
		uv_poll_init(uv_default_loop(), watcher, fd);
		uv_poll_start(watcher, events, watcher_handle);
		uv_unref((uv_handle_t *)watcher);

		dbus_watch_set_data(watch, (void *)watcher, watcher_free);

		return true;
	}

	static void watch_remove(DBusWatch *watch, void *data)
	{
		uv_poll_t *watcher = (uv_poll_t *)dbus_watch_get_data(watch);

		// Stop watching
		uv_ref((uv_handle_t *)watcher);
		uv_poll_stop(watcher);
		uv_close((uv_handle_t *)watcher, NULL);

		dbus_watch_set_data(watch, NULL, NULL);
	}

	static void watch_handle(DBusWatch *watch, void *data)
	{
		if (dbus_watch_get_enabled(watch))
			watch_add(watch, data);
		else
			watch_remove(watch, data);
	}

	static void timer_handle(uv_timer_t *timer, int status)
	{
		DBusTimeout *timeout = (DBusTimeout*)timer->data;
		dbus_timeout_handle(timeout);
	}

	static void timer_free(void *data)
	{
		uv_timer_t *timer = (uv_timer_t *)data;
		timer->data =  NULL;

		free(timer);
	}

	static dbus_bool_t timeout_add(DBusTimeout *timeout, void *data)
	{ 
		if (!dbus_timeout_get_enabled(timeout) || dbus_timeout_get_data(timeout) != NULL)
			return true;

		uv_timer_t *timer = new uv_timer_t;
		timer->data = timeout;

		// Initializing timer
		uv_timer_init(uv_default_loop(), timer);
		uv_timer_start(timer, timer_handle, dbus_timeout_get_interval(timeout), 0);

		dbus_timeout_set_data(timeout, (void *)timer, timer_free);

		return true;
	}

	static void timeout_remove(DBusTimeout *timeout, void *data)
	{
		uv_timer_t *timer = (uv_timer_t *)dbus_timeout_get_data(timeout);

		// Stop timer
		uv_timer_stop(timer);
		uv_unref((uv_handle_t *)timer);

		dbus_timeout_set_data(timeout, NULL, NULL);
	}

	static void timeout_handle(DBusTimeout *timeout, void *data)
	{
		if (dbus_timeout_get_enabled(timeout))
			timeout_add(timeout, data);
		else
			timeout_remove(timeout, data);
	}

	static void connection_wakeup(void *data)
	{
		uv_async_t *connection_loop = (uv_async_t *)data;
		uv_async_send(connection_loop);
	}

	static void connection_loop(uv_async_t *connection_loop, int status)
	{
		DBusConnection *connection = (DBusConnection *)connection_loop->data;
		dbus_connection_read_write(connection, 0);

		while(dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS);
	}

	void Init(DBusConnection *connection)
	{
		dbus_connection_set_exit_on_disconnect(connection, false);

		// Initializing watcher
		dbus_connection_set_watch_functions(connection, watch_add, watch_remove, watch_handle, NULL, NULL);

		// Initializing timeout handlers
		dbus_connection_set_timeout_functions(connection, timeout_add, timeout_remove, timeout_handle, NULL, NULL);

		// Initializing loop
		uv_async_t *connection_loop_handle = new uv_async_t;
		connection_loop_handle->data = (void *)connection;
		uv_async_init(uv_default_loop(), connection_loop_handle, connection_loop);
		//uv_unref((uv_handle_t *)connection_loop_handle);

		dbus_connection_set_wakeup_main_function(connection, connection_wakeup, connection_loop_handle, free);
	}

}
