#!/usr/bin/env python3

"""
Folder Rearrange & Media Recommender
A GUI application that randomly rearranges files in a folder and suggests media files to view.

Features:
- Random file shuffling in selected folder
- Media file detection and filtering
- Random recommendations with preview
- Simple GUI interface
- Support for common media formats
"""

import os
import random
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pathlib import Path
import subprocess
import platform

# Media file extensions
MEDIA_EXTENSIONS = {
    # Video formats
    '.mp4', '.avi', '.mov', '.mkv', '.webm', '.flv', '.vob', '.ogv', '.ogg',
    '.wmv', '.mpg', '.mpeg', '.m4v', '.mpv', '.f4v', '.swf', '.avchd', '.qt',
    '.3gp', '.mpeg-1', '.mpeg-2', '.mpeg4', '.html5',
    # Audio formats
    '.mp3', '.wav', '.flac', '.aac', '.ogg', '.wma', '.m4a', '.opus',
    # Image formats
    '.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff', '.webp', '.svg',
    # Document formats (for viewing)
    '.pdf', '.txt', '.md', '.html', '.htm'
}

class FolderRearranger:
    def __init__(self, root):
        self.root = root
        self.root.title("Folder Rearrange & Media Recommender")
        self.root.geometry("800x600")
        self.root.configure(bg='#f0f0f0')
        
        # Variables
        self.selected_folder = tk.StringVar()
        self.media_files = []
        self.recommendations = []
        
        self.setup_ui()
        
    def setup_ui(self):
        """Setup the user interface"""
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        
        # Title
        title_label = ttk.Label(main_frame, text="Folder Rearrange & Media Recommender", 
                               font=('Arial', 16, 'bold'))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Folder selection section
        folder_frame = ttk.LabelFrame(main_frame, text="Folder Selection", padding="10")
        folder_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        folder_frame.columnconfigure(1, weight=1)
        
        ttk.Label(folder_frame, text="Select Folder:").grid(row=0, column=0, sticky=tk.W, padx=(0, 10))
        
        folder_entry = ttk.Entry(folder_frame, textvariable=self.selected_folder, width=50)
        folder_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), padx=(0, 10))
        
        browse_btn = ttk.Button(folder_frame, text="Browse", command=self.browse_folder)
        browse_btn.grid(row=0, column=2)
        
        # Action buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.grid(row=2, column=0, columnspan=3, pady=10)
        
        self.scan_btn = ttk.Button(button_frame, text="Scan Folder", command=self.scan_folder)
        self.scan_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.shuffle_btn = ttk.Button(button_frame, text="Shuffle Files", command=self.shuffle_files)
        self.shuffle_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        self.recommend_btn = ttk.Button(button_frame, text="Get Recommendations", command=self.get_recommendations)
        self.recommend_btn.pack(side=tk.LEFT)
        
        # Status label
        self.status_label = ttk.Label(main_frame, text="Select a folder to begin", foreground='blue')
        self.status_label.grid(row=3, column=0, columnspan=3, pady=(0, 10))
        
        # Results section
        results_frame = ttk.LabelFrame(main_frame, text="Results", padding="10")
        results_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)
        main_frame.rowconfigure(4, weight=1)
        
        # Create notebook for tabs
        self.notebook = ttk.Notebook(results_frame)
        self.notebook.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Media files tab
        self.media_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.media_frame, text="Media Files")
        
        # Media files listbox with scrollbar
        media_list_frame = ttk.Frame(self.media_frame)
        media_list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.media_listbox = tk.Listbox(media_list_frame, height=15)
        media_scrollbar = ttk.Scrollbar(media_list_frame, orient=tk.VERTICAL, command=self.media_listbox.yview)
        self.media_listbox.configure(yscrollcommand=media_scrollbar.set)
        
        self.media_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        media_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Recommendations tab
        self.recommendations_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.recommendations_frame, text="Recommendations")
        
        # Recommendations listbox with scrollbar
        rec_list_frame = ttk.Frame(self.recommendations_frame)
        rec_list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.rec_listbox = tk.Listbox(rec_list_frame, height=15)
        rec_scrollbar = ttk.Scrollbar(rec_list_frame, orient=tk.VERTICAL, command=self.rec_listbox.yview)
        self.rec_listbox.configure(yscrollcommand=rec_scrollbar.set)
        
        self.rec_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        rec_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Double-click handlers
        self.media_listbox.bind('<Double-1>', self.open_media_file)
        self.rec_listbox.bind('<Double-1>', self.open_media_file)
        
        # Action buttons for recommendations
        rec_button_frame = ttk.Frame(self.recommendations_frame)
        rec_button_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(rec_button_frame, text="Open Selected", command=self.open_selected_file).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(rec_button_frame, text="Refresh Recommendations", command=self.get_recommendations).pack(side=tk.LEFT)
        
    def browse_folder(self):
        """Open folder selection dialog"""
        folder = filedialog.askdirectory(title="Select Folder to Rearrange")
        if folder:
            self.selected_folder.set(folder)
            self.status_label.config(text=f"Selected folder: {folder}", foreground='green')
            
    def scan_folder(self):
        """Scan the selected folder for media files"""
        folder_path = self.selected_folder.get()
        if not folder_path or not os.path.exists(folder_path):
            messagebox.showerror("Error", "Please select a valid folder first")
            return
            
        self.media_files = []
        self.media_listbox.delete(0, tk.END)
        
        try:
            for root, dirs, files in os.walk(folder_path):
                for file in files:
                    file_path = os.path.join(root, file)
                    file_ext = os.path.splitext(file)[1].lower()
                    
                    if file_ext in MEDIA_EXTENSIONS:
                        # Get relative path from selected folder
                        rel_path = os.path.relpath(file_path, folder_path)
                        self.media_files.append({
                            'name': file,
                            'path': file_path,
                            'relative_path': rel_path,
                            'extension': file_ext,
                            'size': os.path.getsize(file_path)
                        })
            
            # Sort by name for display
            self.media_files.sort(key=lambda x: x['name'].lower())
            
            # Display in listbox
            for media_file in self.media_files:
                size_mb = media_file['size'] / (1024 * 1024)
                display_text = f"{media_file['relative_path']} ({size_mb:.1f} MB)"
                self.media_listbox.insert(tk.END, display_text)
            
            self.status_label.config(text=f"Found {len(self.media_files)} media files", foreground='green')
            
        except Exception as e:
            messagebox.showerror("Error", f"Error scanning folder: {str(e)}")
            self.status_label.config(text="Error scanning folder", foreground='red')
            
    def shuffle_files(self):
        """Randomly shuffle the files in the folder"""
        folder_path = self.selected_folder.get()
        if not folder_path or not os.path.exists(folder_path):
            messagebox.showerror("Error", "Please select a valid folder first")
            return
            
        try:
            # Get all files in the folder (not subdirectories)
            files = [f for f in os.listdir(folder_path) 
                    if os.path.isfile(os.path.join(folder_path, f))]
            
            if not files:
                messagebox.showinfo("Info", "No files found in the selected folder")
                return
                
            # Create temporary names
            temp_names = []
            for i, file in enumerate(files):
                name, ext = os.path.splitext(file)
                temp_name = f"temp_{i:04d}{ext}"
                temp_names.append(temp_name)
                
            # Rename files to temporary names
            for file, temp_name in zip(files, temp_names):
                old_path = os.path.join(folder_path, file)
                temp_path = os.path.join(folder_path, temp_name)
                os.rename(old_path, temp_path)
                
            # Shuffle the list
            random.shuffle(files)
            
            # Rename files to shuffled names
            for temp_name, new_file in zip(temp_names, files):
                temp_path = os.path.join(folder_path, temp_name)
                new_path = os.path.join(folder_path, new_file)
                os.rename(temp_path, new_path)
                
            self.status_label.config(text=f"Shuffled {len(files)} files in folder", foreground='green')
            messagebox.showinfo("Success", f"Successfully shuffled {len(files)} files!")
            
        except Exception as e:
            messagebox.showerror("Error", f"Error shuffling files: {str(e)}")
            self.status_label.config(text="Error shuffling files", foreground='red')
            
    def get_recommendations(self):
        """Get random recommendations from media files"""
        if not self.media_files:
            messagebox.showwarning("Warning", "No media files found. Please scan the folder first.")
            return
            
        # Get 5 random recommendations (or all if less than 5)
        num_recs = min(5, len(self.media_files))
        self.recommendations = random.sample(self.media_files, num_recs)
        
        # Clear and populate recommendations listbox
        self.rec_listbox.delete(0, tk.END)
        
        for i, media_file in enumerate(self.recommendations, 1):
            size_mb = media_file['size'] / (1024 * 1024)
            display_text = f"{i}. {media_file['relative_path']} ({size_mb:.1f} MB)"
            self.rec_listbox.insert(tk.END, display_text)
            
        self.status_label.config(text=f"Generated {num_recs} random recommendations", foreground='green')
        
    def open_media_file(self, event):
        """Open the selected media file"""
        widget = event.widget
        selection = widget.curselection()
        
        if not selection:
            return
            
        index = selection[0]
        
        # Determine which listbox was clicked
        if widget == self.media_listbox:
            media_file = self.media_files[index]
        else:  # recommendations listbox
            media_file = self.recommendations[index]
            
        self.open_file(media_file['path'])
        
    def open_selected_file(self):
        """Open the currently selected file in recommendations"""
        selection = self.rec_listbox.curselection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a file from the recommendations")
            return
            
        index = selection[0]
        media_file = self.recommendations[index]
        self.open_file(media_file['path'])
        
    def open_file(self, file_path):
        """Open a file with the default system application"""
        try:
            if platform.system() == 'Darwin':  # macOS
                subprocess.run(['open', file_path])
            elif platform.system() == 'Windows':  # Windows
                os.startfile(file_path)
            else:  # Linux and others
                subprocess.run(['xdg-open', file_path])
                
        except Exception as e:
            messagebox.showerror("Error", f"Could not open file: {str(e)}")

def main():
    """Main function to run the application"""
    root = tk.Tk()
    app = FolderRearranger(root)
    
    # Center the window
    root.update_idletasks()
    x = (root.winfo_screenwidth() // 2) - (root.winfo_width() // 2)
    y = (root.winfo_screenheight() // 2) - (root.winfo_height() // 2)
    root.geometry(f"+{x}+{y}")
    
    root.mainloop()

if __name__ == "__main__":
    main()
