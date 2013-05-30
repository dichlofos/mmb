package ru.mmb.terminal.model.registry;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ru.mmb.terminal.db.TerminalDB;
import ru.mmb.terminal.model.User;

public class UsersRegistry
{
	private static UsersRegistry instance = null;

	private final Map<Integer, User> users = new HashMap<Integer, User>();

	public static UsersRegistry getInstance()
	{
		if (instance == null)
		{
			instance = new UsersRegistry();
		}
		return instance;
	}

	private UsersRegistry()
	{
		refresh();
	}

	public void refresh()
	{
		try
		{
			users.clear();
			List<User> loadedUsers = TerminalDB.getInstance().loadUsers();
			for (User user : loadedUsers)
			{
				users.put(new Integer(user.getUserId()), user);
			}
		}
		catch (Exception e)
		{
			throw new RuntimeException("Users list load failed.", e);
		}
	}

	public Map<Integer, User> getUsers()
	{
		return Collections.unmodifiableMap(users);
	}

	public User getUserById(int userId)
	{
		return users.get(new Integer(userId));
	}
}
