import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import struct
import os

# Constants matching C header
SIMT_WIDTH = 16
MAX_BINDINGS = 8
MAX_ATTRIBUTES = 8
MAX_UBO_ELEMENTS = 10
MAX_ELEM_SIZE = SIMT_WIDTH * 4 * 4
MAX_UBO_SIZE = MAX_UBO_ELEMENTS * 4 * 4

# Type definitions
class DataType:
    I32 = 0
    F32 = 1
    Vec3 = 2
    Vec4 = 3
    Mat3 = 4
    Mat4 = 5

TYPE_NAMES = {
    DataType.I32: "I32",
    DataType.F32: "F32",
    DataType.Vec3: "Vec3",
    DataType.Vec4: "Vec4",
    DataType.Mat3: "Mat3",
    DataType.Mat4: "Mat4"
}

TYPE_SIZES = {
    DataType.I32: 1,
    DataType.F32: 1,
    DataType.Vec3: 3,
    DataType.Vec4: 4,
    DataType.Mat3: 9,
    DataType.Mat4: 16
}

class ExecutionContextMap:
    def __init__(self):
        self.binding_buffers_map = [0] * MAX_BINDINGS
        self.location_in_buffers_map = [0] * MAX_ATTRIBUTES
        self.location_out_buffers_map = [0] * MAX_ATTRIBUTES
        
        self.binding_buffers_offset = [0] * MAX_BINDINGS
        self.location_in_buffers_offset = [0] * MAX_ATTRIBUTES
        self.location_out_buffers_offset = [0] * MAX_ATTRIBUTES
        
        self.binding_buffers_size = [0] * MAX_BINDINGS
        self.location_in_buffers_size = [0] * MAX_ATTRIBUTES
        self.location_out_buffers_size = [0] * MAX_ATTRIBUTES
        
        self.binding_buffers = [0] * (MAX_BINDINGS * MAX_UBO_SIZE)
        self.location_in_buffers = [0] * (MAX_ATTRIBUTES * MAX_ELEM_SIZE)
        self.location_out_buffers = [0] * (MAX_ATTRIBUTES * MAX_ELEM_SIZE)
    
    def save_to_file(self, filename):
        try:
            with open(filename, 'wb') as f:
                # Write maps
                f.write(struct.pack(f'{MAX_BINDINGS}I', *self.binding_buffers_map))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_in_buffers_map))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_out_buffers_map))
                
                # Write offsets
                f.write(struct.pack(f'{MAX_BINDINGS}I', *self.binding_buffers_offset))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_in_buffers_offset))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_out_buffers_offset))
                
                # Write sizes
                f.write(struct.pack(f'{MAX_BINDINGS}I', *self.binding_buffers_size))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_in_buffers_size))
                f.write(struct.pack(f'{MAX_ATTRIBUTES}I', *self.location_out_buffers_size))
                
                # Write buffers
                f.write(struct.pack(f'{MAX_BINDINGS * MAX_UBO_SIZE}I', *self.binding_buffers))
                f.write(struct.pack(f'{MAX_ATTRIBUTES * MAX_ELEM_SIZE}I', *self.location_in_buffers))
                f.write(struct.pack(f'{MAX_ATTRIBUTES * MAX_ELEM_SIZE}I', *self.location_out_buffers))
            
            return True
        except Exception as e:
            messagebox.showerror("Save Error", f"Failed to save: {str(e)}")
            return False
    
    def load_from_file(self, filename):
        try:
            with open(filename, 'rb') as f:
                # Read maps
                self.binding_buffers_map = list(struct.unpack(f'{MAX_BINDINGS}I', f.read(MAX_BINDINGS * 4)))
                self.location_in_buffers_map = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                self.location_out_buffers_map = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                
                # Read offsets
                self.binding_buffers_offset = list(struct.unpack(f'{MAX_BINDINGS}I', f.read(MAX_BINDINGS * 4)))
                self.location_in_buffers_offset = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                self.location_out_buffers_offset = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                
                # Read sizes
                self.binding_buffers_size = list(struct.unpack(f'{MAX_BINDINGS}I', f.read(MAX_BINDINGS * 4)))
                self.location_in_buffers_size = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                self.location_out_buffers_size = list(struct.unpack(f'{MAX_ATTRIBUTES}I', f.read(MAX_ATTRIBUTES * 4)))
                
                # Read buffers
                self.binding_buffers = list(struct.unpack(f'{MAX_BINDINGS * MAX_UBO_SIZE}I', f.read(MAX_BINDINGS * MAX_UBO_SIZE * 4)))
                self.location_in_buffers = list(struct.unpack(f'{MAX_ATTRIBUTES * MAX_ELEM_SIZE}I', f.read(MAX_ATTRIBUTES * MAX_ELEM_SIZE * 4)))
                self.location_out_buffers = list(struct.unpack(f'{MAX_ATTRIBUTES * MAX_ELEM_SIZE}I', f.read(MAX_ATTRIBUTES * MAX_ELEM_SIZE * 4)))
            
            return True
        except Exception as e:
            messagebox.showerror("Load Error", f"Failed to load: {str(e)}")
            return False


class BindingEditorWindow:
    """Editor for binding buffers using struct definition"""
    def __init__(self, parent, buffer_index, struct_def, initial_data, callback):
        self.window = tk.Toplevel(parent)
        self.window.title(f"Binding Buffer [{buffer_index}] Editor")
        self.window.geometry("500x400")
        self.callback = callback
        self.struct_def = struct_def
        self.initial_data = initial_data
        
        self.value_entries = []
        self.create_ui()
    
    def create_ui(self):
        frame = ttk.LabelFrame(self.window, text="Edit Struct Values")
        frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        row = 0
        total_idx = 0
        for name, dtype in self.struct_def:
            size = TYPE_SIZES[dtype]
            ttk.Label(frame, text=f"{name} ({TYPE_NAMES[dtype]})").grid(row=row, column=0, columnspan=2, sticky=tk.W, padx=5, pady=5)
            row += 1
            
            entries = []
            for j in range(size):
                ttk.Label(frame, text=f"  [{j}]").grid(row=row, column=0, sticky=tk.W, padx=20, pady=2)
                entry = ttk.Entry(frame, width=15)
                if total_idx < len(self.initial_data):
                    entry.insert(0, str(self.initial_data[total_idx]))
                else:
                    entry.insert(0, "0.0")
                entry.grid(row=row, column=1, padx=5, pady=2)
                entries.append(entry)
                total_idx += 1
                row += 1
            
            self.value_entries.append(entries)
        
        button_frame = ttk.Frame(self.window)
        button_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(button_frame, text="Apply", command=self.apply).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=self.window.destroy).pack(side=tk.LEFT, padx=5)
    
    def apply(self):
        try:
            data = []
            for member_idx, (name, dtype) in enumerate(self.struct_def):
                entries = self.value_entries[member_idx]
                for entry in entries:
                    val_str = entry.get()
                    
                    if dtype == DataType.I32:
                        # Integer value
                        int_val = int(val_str)
                        data.append(int_val & 0xFFFFFFFF)
                    else:
                        # Float value - convert to uint32 bit representation
                        float_val = float(val_str)
                        uint32_bits = struct.unpack('I', struct.pack('f', float_val))[0]
                        data.append(uint32_bits)
            
            self.callback(data)
            self.window.destroy()
        except ValueError as e:
            messagebox.showerror("Error", f"Invalid value: {str(e)}")


class LocationEditorWindow:
    """Editor for location buffers - edit per lane based on type"""
    def __init__(self, parent, buffer_index, buffer_type, dtype, initial_data, callback):
        self.window = tk.Toplevel(parent)
        self.window.title(f"{buffer_type.title()} Location [{buffer_index}] - Type: {TYPE_NAMES[dtype]}")
        self.window.geometry("600x500")
        self.callback = callback
        self.buffer_type = buffer_type
        self.dtype = dtype
        self.size = TYPE_SIZES[dtype]
        
        self.lane_entries = {}
        self.splat_var = tk.BooleanVar(value=False)
        self.create_ui(initial_data)
    
    def create_ui(self, initial_data):
        # Splat option frame
        splat_frame = ttk.LabelFrame(self.window, text="Options")
        splat_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Checkbutton(splat_frame, text="Splat Lane 0 across all lanes", variable=self.splat_var).pack(padx=10, pady=5)
        
        # Create scrollable frame for lanes
        canvas = tk.Canvas(self.window)
        scrollbar = ttk.Scrollbar(self.window, orient=tk.VERTICAL, command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)
        
        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        # Create lane editors
        for lane in range(SIMT_WIDTH):
            lane_frame = ttk.LabelFrame(scrollable_frame, text=f"Lane {lane}")
            lane_frame.pack(fill=tk.X, padx=5, pady=2)
            
            self.lane_entries[lane] = []
            for j in range(self.size):
                ttk.Label(lane_frame, text=f"[{j}]").grid(row=j, column=0, padx=10, pady=2, sticky=tk.W)
                entry = ttk.Entry(lane_frame, width=15)
                
                # Get initial value
                idx = lane * self.size + j
                if idx < len(initial_data):
                    entry.insert(0, str(initial_data[idx]))
                else:
                    entry.insert(0, "0.0")
                
                entry.grid(row=j, column=1, padx=5, pady=2)
                self.lane_entries[lane].append(entry)
        
        canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Buttons
        button_frame = ttk.Frame(self.window)
        button_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(button_frame, text="Apply", command=self.apply).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=self.window.destroy).pack(side=tk.LEFT, padx=5)
    
    def apply(self):
        try:
            data = []
            
            if self.splat_var.get():
                # Get lane 0 values and splat across all lanes
                lane0_values = []
                for entry in self.lane_entries[0]:
                    val_str = entry.get()
                    
                    if self.dtype == DataType.I32:
                        int_val = int(val_str)
                        lane0_values.append(int_val & 0xFFFFFFFF)
                    else:
                        float_val = float(val_str)
                        uint32_bits = struct.unpack('I', struct.pack('f', float_val))[0]
                        lane0_values.append(uint32_bits)
                
                # Replicate lane 0 across all lanes
                for lane in range(SIMT_WIDTH):
                    data.extend(lane0_values)
            else:
                # Collect all lane values
                for lane in range(SIMT_WIDTH):
                    for entry in self.lane_entries[lane]:
                        val_str = entry.get()
                        
                        if self.dtype == DataType.I32:
                            int_val = int(val_str)
                            data.append(int_val & 0xFFFFFFFF)
                        else:
                            float_val = float(val_str)
                            uint32_bits = struct.unpack('I', struct.pack('f', float_val))[0]
                            data.append(uint32_bits)
            
            self.callback(data)
            self.window.destroy()
        except ValueError as e:
            messagebox.showerror("Error", f"Invalid value: {str(e)}")


class ContextMapGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("ExecutionContextMap Editor")
        self.root.geometry("1200x700")
        
        self.context = ExecutionContextMap()
        self.binding_struct_defs = {}  # {index: [(name, type), ...]}
        self.location_dtypes = {}  # {(buffer_type, index): dtype}
        
        # Create menu bar
        menubar = tk.Menu(root)
        root.config(menu=menubar)
        
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_context)
        file_menu.add_command(label="Load", command=self.load_context)
        file_menu.add_command(label="Save", command=self.save_context)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=root.quit)
        
        # Create notebook for tabs
        self.notebook = ttk.Notebook(root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Tab 0: Binding Buffers
        self.binding_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.binding_frame, text="Binding Buffers")
        self.create_binding_tab()
        
        # Tab 1: Input Location Buffers
        self.input_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.input_frame, text="Input Locations")
        self.create_input_tab()
        
        # Tab 2: Output Location Buffers
        self.output_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.output_frame, text="Output Locations")
        self.create_output_tab()
        
        # Tab 3: View Data
        self.view_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.view_frame, text="View Data")
        self.create_view_tab()
    
    def create_binding_tab(self):
        frame = ttk.LabelFrame(self.binding_frame, text="Binding Buffers")
        frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        ttk.Label(frame, text="Index").grid(row=0, column=0, padx=5, pady=5)
        ttk.Label(frame, text="Active").grid(row=0, column=1, padx=5, pady=5)
        ttk.Label(frame, text="Offset").grid(row=0, column=2, padx=5, pady=5)
        ttk.Label(frame, text="Size").grid(row=0, column=3, padx=5, pady=5)
        ttk.Label(frame, text="Actions").grid(row=0, column=4, padx=5, pady=5)
        
        self.binding_widgets = []
        for i in range(MAX_BINDINGS):
            widgets = {}
            ttk.Label(frame, text=str(i)).grid(row=i+1, column=0, padx=5, pady=2)
            
            widgets['active'] = tk.BooleanVar()
            ttk.Checkbutton(frame, variable=widgets['active']).grid(row=i+1, column=1, padx=5, pady=2)
            
            widgets['offset'] = ttk.Spinbox(frame, from_=0, to=65536, width=10)
            widgets['offset'].grid(row=i+1, column=2, padx=5, pady=2)
            
            widgets['size'] = ttk.Spinbox(frame, from_=0, to=1024, width=10)
            widgets['size'].grid(row=i+1, column=3, padx=5, pady=2)
            
            button_frame = ttk.Frame(frame)
            button_frame.grid(row=i+1, column=4, padx=5, pady=2)
            ttk.Button(button_frame, text="Edit", command=lambda idx=i: self.edit_binding(idx)).pack(side=tk.LEFT, padx=2)
            ttk.Button(button_frame, text="DefineStruct", command=lambda idx=i: self.define_binding_struct(idx)).pack(side=tk.LEFT, padx=2)
            
            self.binding_widgets.append(widgets)
    
    def create_input_tab(self):
        frame = ttk.LabelFrame(self.input_frame, text="Input Location Buffers")
        frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        ttk.Label(frame, text="Index").grid(row=0, column=0, padx=5, pady=5)
        ttk.Label(frame, text="Active").grid(row=0, column=1, padx=5, pady=5)
        ttk.Label(frame, text="Type").grid(row=0, column=2, padx=5, pady=5)
        ttk.Label(frame, text="Offset").grid(row=0, column=3, padx=5, pady=5)
        ttk.Label(frame, text="Size").grid(row=0, column=4, padx=5, pady=5)
        ttk.Label(frame, text="Actions").grid(row=0, column=5, padx=5, pady=5)
        
        self.input_widgets = []
        for i in range(MAX_ATTRIBUTES):
            widgets = {}
            ttk.Label(frame, text=str(i)).grid(row=i+1, column=0, padx=5, pady=2)
            
            widgets['active'] = tk.BooleanVar()
            ttk.Checkbutton(frame, variable=widgets['active']).grid(row=i+1, column=1, padx=5, pady=2)
            
            widgets['type'] = ttk.Combobox(frame, values=list(TYPE_NAMES.values()), state="readonly", width=8)
            widgets['type'].set("F32")
            widgets['type'].grid(row=i+1, column=2, padx=5, pady=2)
            
            widgets['offset'] = ttk.Spinbox(frame, from_=0, to=65536, width=8)
            widgets['offset'].grid(row=i+1, column=3, padx=5, pady=2)
            
            widgets['size'] = ttk.Spinbox(frame, from_=0, to=1024, width=8)
            widgets['size'].grid(row=i+1, column=4, padx=5, pady=2)
            
            button_frame = ttk.Frame(frame)
            button_frame.grid(row=i+1, column=5, padx=5, pady=2)
            ttk.Button(button_frame, text="Edit", command=lambda idx=i: self.edit_location('input', idx)).pack(side=tk.LEFT, padx=2)
            
            self.input_widgets.append(widgets)
    
    def create_output_tab(self):
        frame = ttk.LabelFrame(self.output_frame, text="Output Location Buffers")
        frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        ttk.Label(frame, text="Index").grid(row=0, column=0, padx=5, pady=5)
        ttk.Label(frame, text="Active").grid(row=0, column=1, padx=5, pady=5)
        ttk.Label(frame, text="Type").grid(row=0, column=2, padx=5, pady=5)
        ttk.Label(frame, text="Offset").grid(row=0, column=3, padx=5, pady=5)
        ttk.Label(frame, text="Size").grid(row=0, column=4, padx=5, pady=5)
        ttk.Label(frame, text="Actions").grid(row=0, column=5, padx=5, pady=5)
        
        self.output_widgets = []
        for i in range(MAX_ATTRIBUTES):
            widgets = {}
            ttk.Label(frame, text=str(i)).grid(row=i+1, column=0, padx=5, pady=2)
            
            widgets['active'] = tk.BooleanVar()
            ttk.Checkbutton(frame, variable=widgets['active']).grid(row=i+1, column=1, padx=5, pady=2)
            
            widgets['type'] = ttk.Combobox(frame, values=list(TYPE_NAMES.values()), state="readonly", width=8)
            widgets['type'].set("F32")
            widgets['type'].grid(row=i+1, column=2, padx=5, pady=2)
            
            widgets['offset'] = ttk.Spinbox(frame, from_=0, to=65536, width=8)
            widgets['offset'].grid(row=i+1, column=3, padx=5, pady=2)
            
            widgets['size'] = ttk.Spinbox(frame, from_=0, to=1024, width=8)
            widgets['size'].grid(row=i+1, column=4, padx=5, pady=2)
            
            button_frame = ttk.Frame(frame)
            button_frame.grid(row=i+1, column=5, padx=5, pady=2)
            ttk.Button(button_frame, text="Edit", command=lambda idx=i: self.edit_location('output', idx)).pack(side=tk.LEFT, padx=2)
            
            self.output_widgets.append(widgets)
    
    def create_view_tab(self):
        button_frame = ttk.Frame(self.view_frame)
        button_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(button_frame, text="Refresh", command=self.refresh_view).pack(side=tk.LEFT, padx=5)
        
        self.view_text = tk.Text(self.view_frame, height=30, width=150, font=("Courier", 9))
        self.view_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
    
    def define_binding_struct(self, index):
        """Define struct for binding buffer"""
        window = tk.Toplevel(self.root)
        window.title(f"Define Struct - Binding [{index}]")
        window.geometry("400x300")
        
        frame = ttk.LabelFrame(window, text="Members")
        frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        entries = []
        for i in range(5):
            name_var = tk.StringVar()
            type_var = tk.StringVar(value="F32")
            
            ttk.Label(frame, text="Name").grid(row=i, column=0, padx=5, pady=2, sticky=tk.W)
            ttk.Entry(frame, textvariable=name_var, width=20).grid(row=i, column=1, padx=5, pady=2)
            
            ttk.Combobox(frame, textvariable=type_var, values=list(TYPE_NAMES.values()), state="readonly", width=10).grid(row=i, column=2, padx=5, pady=2)
            
            entries.append((name_var, type_var))
        
        def confirm():
            struct_def = []
            for name_var, type_var in entries:
                name = name_var.get().strip()
                if name:
                    type_str = type_var.get()
                    dtype = {v: k for k, v in TYPE_NAMES.items()}[type_str]
                    struct_def.append((name, dtype))
            
            if struct_def:
                self.binding_struct_defs[index] = struct_def
                messagebox.showinfo("Success", f"Struct defined with {len(struct_def)} members")
            window.destroy()
        
        button_frame = ttk.Frame(window)
        button_frame.pack(fill=tk.X, padx=5, pady=5)
        ttk.Button(button_frame, text="Confirm", command=confirm).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=window.destroy).pack(side=tk.LEFT, padx=5)
    
    def edit_binding(self, index):
        """Edit binding buffer data"""
        if index not in self.binding_struct_defs:
            messagebox.showwarning("Warning", f"Define struct first for binding [{index}]")
            return
        
        struct_def = self.binding_struct_defs[index]
        start = index * MAX_UBO_SIZE
        current_data = self.context.binding_buffers[start:start+20]
        
        def callback(data):
            # Splat each struct member across SIMT_WIDTH lanes
            splatted = []
            for val in data:
                for lane in range(SIMT_WIDTH):
                    splatted.append(val)
            
            for i, val in enumerate(splatted):
                if i < MAX_UBO_SIZE:
                    self.context.binding_buffers[start + i] = val
            self.context.binding_buffers_size[index] = len(splatted) * 4
            self.context.binding_buffers_map[index] = 1
            self.update_ui_from_context()
        
        BindingEditorWindow(self.root, index, struct_def, current_data, callback)
    
    def edit_location(self, buffer_type, index):
        """Edit location buffer data per lane"""
        if buffer_type == 'input':
            widgets = self.input_widgets[index]
            buffers = self.context.location_in_buffers
            maps = self.context.location_in_buffers_map
            sizes = self.context.location_in_buffers_size
        else:
            widgets = self.output_widgets[index]
            buffers = self.context.location_out_buffers
            maps = self.context.location_out_buffers_map
            sizes = self.context.location_out_buffers_size
        
        type_str = widgets['type'].get()
        dtype = {v: k for k, v in TYPE_NAMES.items()}[type_str]
        
        start = index * MAX_ELEM_SIZE
        current_data = buffers[start:start+MAX_ELEM_SIZE]
        
        def callback(data):
            for i, val in enumerate(data):
                if i < MAX_ELEM_SIZE:
                    buffers[start + i] = val
            sizes[index] = len(data) * 4
            maps[index] = 1
            self.location_dtypes[(buffer_type, index)] = dtype
            self.update_ui_from_context()
        
        LocationEditorWindow(self.root, index, buffer_type, dtype, current_data, callback)
    
    def update_ui_from_context(self):
        for i, w in enumerate(self.binding_widgets):
            w['active'].set(self.context.binding_buffers_map[i] == 1)
            w['offset'].delete(0, tk.END)
            w['offset'].insert(0, str(self.context.binding_buffers_offset[i]))
            w['size'].delete(0, tk.END)
            w['size'].insert(0, str(self.context.binding_buffers_size[i]))
        
        for i, w in enumerate(self.input_widgets):
            w['active'].set(self.context.location_in_buffers_map[i] == 1)
            w['offset'].delete(0, tk.END)
            w['offset'].insert(0, str(self.context.location_in_buffers_offset[i]))
            w['size'].delete(0, tk.END)
            w['size'].insert(0, str(self.context.location_in_buffers_size[i]))
        
        for i, w in enumerate(self.output_widgets):
            w['active'].set(self.context.location_out_buffers_map[i] == 1)
            w['offset'].delete(0, tk.END)
            w['offset'].insert(0, str(self.context.location_out_buffers_offset[i]))
            w['size'].delete(0, tk.END)
            w['size'].insert(0, str(self.context.location_out_buffers_size[i]))
    
    def refresh_view(self):
        self.update_context_from_ui()
        self.view_text.delete(1.0, tk.END)
        
        output = "=== EXECUTION CONTEXT MAP ===\n\n"
        
        output += "BINDING BUFFERS:\n"
        for i in range(MAX_BINDINGS):
            if self.context.binding_buffers_map[i]:
                vals = self.context.binding_buffers[i*MAX_UBO_SIZE:i*MAX_UBO_SIZE+4]
                vals_str = " ".join(f"0x{int(v) & 0xFFFFFFFF:08x}" for v in vals)
                output += f"  [{i}] offset=0x{self.context.binding_buffers_offset[i]:04x} size={self.context.binding_buffers_size[i]} values: {vals_str}\n"
        
        output += "\nINPUT LOCATION BUFFERS:\n"
        for i in range(MAX_ATTRIBUTES):
            if self.context.location_in_buffers_map[i]:
                vals = self.context.location_in_buffers[i*MAX_ELEM_SIZE:i*MAX_ELEM_SIZE+4]
                vals_str = " ".join(f"0x{int(v) & 0xFFFFFFFF:08x}" for v in vals)
                output += f"  [{i}] offset=0x{self.context.location_in_buffers_offset[i]:04x} size={self.context.location_in_buffers_size[i]} values: {vals_str}\n"
        
        output += "\nOUTPUT LOCATION BUFFERS:\n"
        for i in range(MAX_ATTRIBUTES):
            if self.context.location_out_buffers_map[i]:
                vals = self.context.location_out_buffers[i*MAX_ELEM_SIZE:i*MAX_ELEM_SIZE+4]
                vals_str = " ".join(f"0x{int(v) & 0xFFFFFFFF:08x}" for v in vals)
                output += f"  [{i}] offset=0x{self.context.location_out_buffers_offset[i]:04x} size={self.context.location_out_buffers_size[i]} values: {vals_str}\n"
        
        self.view_text.insert(tk.END, output)
    
    def update_context_from_ui(self):
        for i, w in enumerate(self.binding_widgets):
            self.context.binding_buffers_map[i] = 1 if w['active'].get() else 0
            self.context.binding_buffers_offset[i] = int(w['offset'].get() or 0)
            self.context.binding_buffers_size[i] = int(w['size'].get() or 0)
        
        for i, w in enumerate(self.input_widgets):
            self.context.location_in_buffers_map[i] = 1 if w['active'].get() else 0
            self.context.location_in_buffers_offset[i] = int(w['offset'].get() or 0)
            self.context.location_in_buffers_size[i] = int(w['size'].get() or 0)
        
        for i, w in enumerate(self.output_widgets):
            self.context.location_out_buffers_map[i] = 1 if w['active'].get() else 0
            self.context.location_out_buffers_offset[i] = int(w['offset'].get() or 0)
            self.context.location_out_buffers_size[i] = int(w['size'].get() or 0)
    
    def new_context(self):
        self.context = ExecutionContextMap()
        self.binding_struct_defs = {}
        self.location_dtypes = {}
        self.update_ui_from_context()
        messagebox.showinfo("New", "New context created")
    
    def save_context(self):
        self.update_context_from_ui()
        filename = filedialog.asksaveasfilename(defaultextension=".bin", filetypes=[("Binary", "*.bin"), ("All", "*.*")])
        if filename and self.context.save_to_file(filename):
            messagebox.showinfo("Save", f"Saved to {filename}")
    
    def load_context(self):
        filename = filedialog.askopenfilename(filetypes=[("Binary", "*.bin"), ("All", "*.*")])
        if filename and self.context.load_from_file(filename):
            self.update_ui_from_context()
            messagebox.showinfo("Load", f"Loaded from {filename}")


if __name__ == "__main__":
    root = tk.Tk()
    app = ContextMapGUI(root)
    root.mainloop()