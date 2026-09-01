import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import warnings
import re

def get_time_from_rtc(series):
    """
    Derive a time axis (in milliseconds) from an RTC column.
    """
    if series.empty:
        return None

    if series.dtype == object:
        series = series.astype(str).str.replace('[', '', regex=False).str.replace(']', '', regex=False)

    t_series = None

    if pd.api.types.is_numeric_dtype(series):
        first_val = series.iloc[0]
        if 30000 < first_val < 70000:
            try: t_series = pd.to_datetime(series, unit='D', origin='1899-12-30')
            except: pass
        elif first_val > 1e11:
            try: t_series = pd.to_datetime(series, unit='ms')
            except: pass
        elif first_val > 1e8:
            try: t_series = pd.to_datetime(series, unit='s')
            except: pass
            
        if t_series is None:
            vals = series.values
            return (vals - vals[0]) * 1000.0

    if t_series is None:
        try:
            s_clean = series.astype(str).str.strip().str.replace(',', '.', regex=False)
            s_num = pd.to_numeric(s_clean, errors='coerce')
            if s_num.notna().sum() > 0.5 * len(series):
                first_val = s_num.dropna().iloc[0]
                if 30000 < first_val < 70000:
                    t_series = pd.to_datetime(s_num, unit='D', origin='1899-12-30')
                elif first_val > 1e11:
                    t_series = pd.to_datetime(s_num, unit='ms')
                elif first_val > 1e8:
                    t_series = pd.to_datetime(s_num, unit='s')
                else:
                    vals = s_num.interpolate().bfill().ffill().values
                    return (vals - vals[0]) * 1000.0
        except:
            pass

    if t_series is None:
        for fmt in ['%Y-%m-%d-%H-%M-%S.%f', '%Y-%m-%d-%H-%M-%S']:
            try:
                t_series = pd.to_datetime(series, format=fmt, errors='raise')
                break
            except: pass

    if t_series is None:
        try:
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning, message=".*Could not infer format.*")
                t_series = pd.to_datetime(series, errors='coerce', dayfirst=True)
        except: return None

    try:
        if t_series is None or t_series.isna().all(): return None
        if t_series.isna().any(): t_series = t_series.interpolate().bfill().ffill()

        t_ms = t_series.astype(np.int64) // 10**6
        t_ms = t_ms.values
        t_ms = t_ms - t_ms[0]
        
        if len(series) > 10:
            unique_vals, indices = np.unique(t_ms, return_index=True)
            if len(unique_vals) < len(series) * 0.5:
                t_interp = np.zeros(len(series), dtype=np.float64)
                for i in range(len(unique_vals) - 1):
                    start_idx = indices[i]
                    end_idx = indices[i+1]
                    t_interp[start_idx:end_idx] = np.linspace(unique_vals[i], unique_vals[i+1], end_idx - start_idx, endpoint=False)
                last_idx = indices[-1]
                step = (unique_vals[-1] - unique_vals[-2]) if len(unique_vals) > 1 else 1000.0
                t_interp[last_idx:] = np.linspace(unique_vals[-1], unique_vals[-1] + step, len(series) - last_idx, endpoint=False)
                return t_interp
        
        return t_ms.astype(np.float64)
    except:
        return None

def plot_interactive_run(run_id, df_lg, ignore_cols):
    """Creates a single plot with a clickable interactive legend for ONE run."""
    df_plot = df_lg.copy()
    if df_plot.empty:
        return

    valid_signals = [col for col in df_plot.columns if col not in ignore_cols]
    
    if 'yaw' in df_plot.columns:
        df_plot['yaw'] = df_plot['yaw'] - df_plot['yaw'].iloc[0]
        df_plot['yaw'] = df_plot['yaw'].abs()
    if 'pitch' in df_plot.columns:
        df_plot['pitch'] = -df_plot['pitch']

    fig, ax = plt.subplots(figsize=(14, 8))
    fig.subplots_adjust(right=0.75) 
    
    lines = []
    for signal in valid_signals:
        if pd.api.types.is_numeric_dtype(df_plot[signal]):
            line, = ax.plot(df_plot['time_rel'] / 1000.0, df_plot[signal], label=signal)
            lines.append(line)
            
    ax.set_xlabel('Time (s)')
    lg_start = df_lg.iloc[0, 0]
    lg_end = df_lg.iloc[-1, 0]
    ax.set_title(f'Run {run_id}: {lg_start} to {lg_end}\nKlik op een naam in de legenda om de lijn aan/uit te zetten!')
    ax.grid(True)
    
    leg = ax.legend(loc='center left', bbox_to_anchor=(1.02, 0.5), ncol=1)
    
    lined = {}
    for legline, origline in zip(leg.get_lines(), lines):
        legline.set_picker(True) 
        legline.set_pickradius(5) 
        lined[legline] = origline
        
    def on_pick(event):
        legline = event.artist
        origline = lined[legline]
        visible = not origline.get_visible()
        origline.set_visible(visible)
        legline.set_alpha(1.0 if visible else 0.2)
        fig.canvas.draw()
        
    fig.canvas.mpl_connect('pick_event', on_pick)
    plt.show()

def plot_signal_across_runs(signal, runs_logger):
    """Creates a single plot comparing one specific signal across ALL runs."""
    fig, ax = plt.subplots(figsize=(14, 8))
    fig.subplots_adjust(right=0.85) 
    
    lines = []
    plotted_runs = []
    
    for run_id in sorted(runs_logger.keys()):
        df_run = runs_logger[run_id]
        
        if signal in df_run.columns and pd.api.types.is_numeric_dtype(df_run[signal]):
            df_plot = df_run.copy()
            
            # Apply processing for specific signals
            if signal == 'yaw':
                df_plot['yaw'] = df_plot['yaw'] - df_plot['yaw'].iloc[0]
                df_plot['yaw'] = df_plot['yaw'].abs()
            elif signal == 'pitch':
                df_plot['pitch'] = -df_plot['pitch']
                
            line, = ax.plot(df_plot['time_rel'] / 1000.0, df_plot[signal], label=f"Run {run_id}")
            lines.append(line)
            plotted_runs.append(run_id)
            
    if not lines:
        print(f"Could not plot {signal}. Ensure it exists and is numeric.")
        plt.close(fig)
        return
        
    ax.set_xlabel('Time (s)')
    ax.set_title(f'Comparison: {signal} across all runs\nKlik op een Run in de legenda om de lijn aan/uit te zetten!')
    ax.grid(True)
    
    leg = ax.legend(loc='center left', bbox_to_anchor=(1.02, 0.5), ncol=1)
    
    lined = {}
    for legline, origline in zip(leg.get_lines(), lines):
        legline.set_picker(True) 
        legline.set_pickradius(5) 
        lined[legline] = origline
        
    def on_pick(event):
        legline = event.artist
        origline = lined[legline]
        visible = not origline.get_visible()
        origline.set_visible(visible)
        legline.set_alpha(1.0 if visible else 0.2)
        fig.canvas.draw()
        
    fig.canvas.mpl_connect('pick_event', on_pick)
    plt.show()

def process_and_plot():
    data_dir = Path("data").resolve()
    
    if not data_dir.exists() or not data_dir.is_dir():
        print(f"Error: Could not find data directory at {data_dir}")
        return
        
    subfolders = [f for f in data_dir.iterdir() if f.is_dir()]
    
    if not subfolders:
        print(f"No subfolders found in {data_dir}")
        return
        
    print("\n--- Available Datasets ---")
    for i, folder in enumerate(subfolders):
        print(f"[{i}] {folder.name}")
        
    try:
        choice = int(input("\nEnter the number of the subfolder to analyze: "))
        selected_folder = subfolders[choice]
    except (ValueError, IndexError):
        print("Invalid selection. Exiting.")
        return

    csv_files = sorted(list(selected_folder.rglob("DATA*.csv")))
    
    if not csv_files:
        print(f"No DATA*.csv files found in {selected_folder.name}")
        return
        
    print(f"\nFound {len(csv_files)} run files in {selected_folder.name}")

    runs_logger = {}
    all_signals_set = set()
    
    for file_path in csv_files:
        match = re.search(r'DATA(\d+)', file_path.stem, re.IGNORECASE)
        if match:
            run_id = int(match.group(1))
        else:
            continue
            
        print(f"Loading Run {run_id} from {file_path.name}...")
        
        try:
            df_run = pd.read_csv(file_path, sep=';')
            if len(df_run.columns) < 2:
                df_run = pd.read_csv(file_path, sep=',')
        except Exception as e:
            print(f"  -> Failed to read {file_path.name}: {e}")
            continue
            
        if df_run.empty:
            continue
            
        if 'LWS_angle' in df_run.columns:
            df_run['steer'] = df_run['LWS_angle']
        elif 'LWS_ANGLE' in df_run.columns:
            df_run['steer'] = df_run['LWS_ANGLE']

        lg_col = df_run.iloc[:, 0]
        lg_time = get_time_from_rtc(lg_col)
        
        if lg_time is None:
            lg_time = np.arange(len(df_run), dtype=float) * 20.0
            
        if 'yaw' in df_run.columns:
            df_run['yaw'] = np.degrees(np.unwrap(np.radians(df_run['yaw'])))
            
        df_run['time_rel'] = lg_time
        df_run = df_run.sort_values(by='time_rel').reset_index(drop=True)
        df_run['time_rel'] = df_run['time_rel'] - df_run['time_rel'].min()
        
        runs_logger[run_id] = df_run
        all_signals_set.update(df_run.columns)
        
    if not runs_logger:
        print("No valid runs could be processed.")
        return

    ignore_cols = {'time', 'Time', 'rtc', 'time_rel', 'millis', 'run_id', 'flag', 'Unnamed: 0', 'Timestamp', 'timestamp'}
    all_signals = sorted([s for s in all_signals_set if s not in ignore_cols])

    while True:
        print(f"\nAvailable runs: {sorted(runs_logger.keys())}")
        print(f"Available signals: {', '.join(all_signals)}")
        choice = input("\nEnter run ID (for all signals), a signal name (for all runs), or 'q' to quit: ")
        
        if choice.lower() == 'q':
            break
            
        if choice.isdigit() and int(choice) in runs_logger:
            # User wants to see all signals for ONE specific run
            run_id = int(choice)
            plot_interactive_run(run_id, runs_logger[run_id], ignore_cols)
            
        elif choice in all_signals:
            # User wants to see ONE specific signal for ALL runs
            signal = choice
            print(f"Plotting {signal} across all runs...")
            plot_signal_across_runs(signal, runs_logger)
            
        else:
            print("Invalid input. Try entering a valid Run ID or Signal name exactly as written.")

if __name__ == "__main__":
    process_and_plot()