
import pandas as pd, re, datetime as dt, os

FILE_PATH = os.getenv('CSV_PATH', 'restaurant_log6.csv')

df = pd.read_csv(FILE_PATH)

def count_waiting_groups(cell: str) -> int:
    if pd.isna(cell) or not str(cell).strip():
        return 0
    return len(re.findall(r'Group \d+\(size \d+\)', str(cell)))

def count_kitchen_orders(cell: str) -> int:
    if pd.isna(cell) or not str(cell).strip():
        return 0
    return len(re.findall(r'[A-Za-z]+\(Group \d+\)', str(cell)))

# Average waiting queue size
waiting_group_counts = df['Waiting Queue'].apply(count_waiting_groups)
avg_queue_size = waiting_group_counts.mean()

# Peak waiting queue size
peak_queue_size = waiting_group_counts.max()

# Average kitchen queue size
kitchen_queue_counts = df['Kitchen Queue'].apply(count_kitchen_orders)
avg_kitchen_queue_size = kitchen_queue_counts.mean()

# Average number of active waiters
def parse_waiters(cell: str) -> int:
    if pd.isna(cell) or not str(cell).strip():
        return 0
    m = re.match(r'(\d+)', str(cell).strip())
    return int(m.group(1)) if m else 0

avg_active_waiters = df['Waiters'].apply(parse_waiters).mean()

# Tracking waiting and meal times
queue_entry = {}   # group_id -> (timestamp, group_size)
seating_time = {}  # group_id -> (timestamp, group_size)

wait_seconds_total = 0
wait_persons_total = 0

meal_wait_seconds_total = 0
meal_wait_persons_total = 0

for _, row in df.iterrows():
    ts = pd.to_datetime(row['Timestamp'])

    # groups currently in waiting queue
    waiting_cell = str(row['Waiting Queue'])
    for grp_id, size in re.findall(r'Group (\d+)\(size (\d+)\)', waiting_cell):
        gid = int(grp_id); sz = int(size)
        if gid not in queue_entry:
            queue_entry[gid] = (ts, sz)

    # groups currently seated
    tables_cell = str(row['Tables'])
    for grp_id in re.findall(r'Group (\d+)', tables_cell):
        gid = int(grp_id)
        if gid in queue_entry:
            start_ts, sz = queue_entry.pop(gid)
            wait_seconds_total += (ts - start_ts).total_seconds() * sz
            wait_persons_total += sz
            seating_time[gid] = (ts, sz)

    # completed orders
    completed_cell = str(row['Completed Orders'])
    for grp_id in re.findall(r'Group (\d+)', completed_cell):
        gid = int(grp_id)
        if gid in seating_time:
            seat_ts, sz = seating_time.pop(gid)
            meal_wait_seconds_total += (ts - seat_ts).total_seconds() * sz
            meal_wait_persons_total += sz

avg_wait_queue_time = (wait_seconds_total / wait_persons_total) if wait_persons_total else None
avg_meal_wait_time = (meal_wait_seconds_total / meal_wait_persons_total) if meal_wait_persons_total else None

stats = {
    'Average queue size (groups)': round(avg_queue_size, 2),
    'Average time a person waited in queue (seconds)': round(avg_wait_queue_time, 2) if avg_wait_queue_time is not None else 'N/A',
    'Average time a person waited for a meal (seconds)': round(avg_meal_wait_time, 2) if avg_meal_wait_time is not None else 'N/A',
    'Peak queue size (groups)': int(peak_queue_size),
    'Average kitchen queue size (orders)': round(avg_kitchen_queue_size, 2),
    'Average active waiters': round(avg_active_waiters, 2)
}

if __name__ == '__main__':
    for k, v in stats.items():
        print(f"{k}: {v}")
