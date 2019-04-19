package ru.mmb.sportiduinomanager;

import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

import ru.mmb.sportiduinomanager.model.Chips;
import ru.mmb.sportiduinomanager.model.Teams;

/**
 * Provides the list of teams visited a station.
 */
public class TeamListAdapter extends RecyclerView.Adapter<TeamListAdapter.TeamHolder> {
    /**
     * Interface for list item click processing.
     */
    private final OnItemClicked mOnClick;
    /**
     * All teams registered for the raid.
     */
    private final Teams mTeams;
    /**
     * All team visits for connected station (sorted, one last visit per team).
     */
    private final Chips mFlash;

    /**
     * Adapter constructor.
     *
     * @param onClick Interface for click processing in calling activity.
     * @param teams   List of all registered teams from ActivePointActivity
     * @param flash   List of all team visits from ActivePointActivity
     */
    TeamListAdapter(final OnItemClicked onClick, final Teams teams, final Chips flash) {
        super();
        mOnClick = onClick;
        mTeams = teams;
        mFlash = flash;
    }

    /**
     * Create new views (invoked by the layout manager).
     */
    @NonNull
    @Override
    public TeamListAdapter.TeamHolder onCreateViewHolder(@NonNull final ViewGroup viewGroup,
                                                         final int viewType) {
        final View view = LayoutInflater.from(viewGroup.getContext())
                .inflate(R.layout.team_list_item, viewGroup, false);
        return new TeamListAdapter.TeamHolder(view);
    }

    /**
     * Replace the contents of a view (invoked by the layout manager).
     */
    @Override
    public void onBindViewHolder(@NonNull final TeamHolder holder, final int position) {
        // Get index of element of mFlash list to display at this position
        int index = mFlash.size() - position;
        if (index < 0) {
            index = 0;
        }
        // Get team number at this position
        final int teamNumber = mFlash.getTeamNumber(index);
        // Get team name for this number
        String teamName = mTeams.getTeamName(teamNumber);
        if (teamName == null) {
            teamName = holder.itemView.getResources().getString(R.string.unknown);
        }
        // Get members count and team time
        final int teamMask = mFlash.getTeamMask(index);
        int teamMembersCount;
        if (teamMask < 0) {
            teamMembersCount = 0;
        } else {
            teamMembersCount = Teams.getMembersCount(teamMask);
        }
        final Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(mFlash.getTeamTime(index) * 1000);
        final DateFormat format = new SimpleDateFormat("dd.MM HH:mm:ss", Locale.getDefault());
        final String timeTime = format.format(calendar.getTime());
        // Update the contents of the view with that team
        holder.mName.setText(holder.itemView.getResources().getString(R.string.ap_team_name,
                teamNumber, teamName));
        holder.mCount.setText(holder.itemView.getResources().getString(R.string.list_team_count,
                teamMembersCount));
        holder.mTime.setText(holder.itemView.getResources().getString(R.string.list_team_time,
                timeTime));
        // Set my listener for all elements of list item
        // TODO: process click for whole RecyclerView item
        holder.mName.setOnClickListener(view -> mOnClick.onItemClick(holder.getAdapterPosition()));
    }

    /**
     * Return the size of team list (invoked by the layout manager).
     */
    @Override
    public int getItemCount() {
        return mFlash.size();
    }

    /**
     * Declare interface for click processing.
     */
    public interface OnItemClicked {
        /**
         * Implemented in BluetoothActivity class.
         *
         * @param position Position of clicked device in the list of discovered devices
         */
        void onItemClick(int position);
    }

    /**
     * Custom ViewHolder for team_list_item layout.
     */
    class TeamHolder extends RecyclerView.ViewHolder {
        /**
         * Team number and name.
         */
        private final TextView mName;
        /**
         * Current number of members computed from team mask.
         */
        private final TextView mCount;
        /**
         * Time of last visit for the team.
         */
        private final TextView mTime;

        /**
         * Holder for list element containing checkbox with team member name.
         *
         * @param view View of list item
         */
        TeamHolder(final View view) {
            super(view);
            mName = view.findViewById(R.id.list_team_name);
            mCount = view.findViewById(R.id.list_team_count);
            mTime = view.findViewById(R.id.list_team_time);
        }
    }
}
