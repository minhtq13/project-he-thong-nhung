import tkinter as tk  # Import thư viện tkinter để tạo giao diện người dùng
# Import các module cần thiết từ firebase_admin
from firebase_admin import credentials, db, initialize_app
# Import module messagebox từ tkinter để hiển thị hộp thoại thông báo
from tkinter import messagebox
import matplotlib.pyplot as plt  # Import module pyplot từ matplotlib để vẽ biểu đồ
# Import các module ImageTk và Image từ PIL để làm việc với hình ảnh
from PIL import ImageTk, Image
# Import module FigureCanvasTkAgg từ matplotlib.backends.backend_tkagg để tích hợp biểu đồ vào giao diện tkinter
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
# Đường dẫn đến tệp chứa thông tin xác thực từ Firebase
cred = credentials.Certificate(
    'D:/HUST/20222/HTN/project-he-thong-nhung/EmbededUI/demoesp32-1e57a-aab0031ce7e3.json')
# Khởi tạo ứng dụng Firebase với thông tin và URL của Firebase Realtime Database
initialize_app(cred, {
               'databaseURL': 'https://demoesp32-1e57a-default-rtdb.asia-southeast1.firebasedatabase.app/'})

# Tham chiếu đến đường dẫn '/rfid' trong Firebase Realtime Database
ref = db.reference('/rfid')
# Tạo một instance của lớp Tk để tạo cửa sổ giao diện người dùng
root = tk.Tk()
# Đặt tiêu đề của cửa sổ giao diện là "RFID Data Management"
root.title("RFID Data Management")


def insert_data():
    tag = rfid_tag_entry.get()  # Lấy giá trị từ trường nhập liệu 'rfid_tag_entry'
    name = name_entry.get()  # Lấy giá trị từ trường nhập liệu 'name_entry'
    money = money_entry.get()  # Lấy giá trị từ trường nhập liệu 'money_entry'
    # Lấy giá trị từ trường nhập liệu 'vehicle_type_entry'
    vehicle_type = vehicle_type_entry.get()

    ref.child(tag).set({  # Ghi dữ liệu vào Firebase Realtime Database dưới dạng một dictionary
        'Name': name,
        'money': money,
        'type': vehicle_type
    })
    # Hiển thị hộp thoại thông báo thành công
    messagebox.showinfo("Success", "RFID data inserted successfully.")

    # Xóa dữ liệu trong các trường nhập liệu sau khi ghi thành công
    rfid_tag_entry.delete(0, tk.END)
    name_entry.delete(0, tk.END)
    money_entry.delete(0, tk.END)
    vehicle_type_entry.delete(0, tk.END)

    refresh_user_list()  # Cập nhật danh sách người dùng trên giao diện
    refresh_report()  # Cập nhật báo cáo trên giao diện


def edit_data():
    tag = rfid_tag_entry.get()  # Lấy giá trị từ trường nhập liệu 'rfid_tag_entry'
    name = name_entry.get()  # Lấy giá trị từ trường nhập liệu 'name_entry'
    money = money_entry.get()  # Lấy giá trị từ trường nhập liệu 'money_entry'
    # Lấy giá trị từ trường nhập liệu 'vehicle_type_entry'
    vehicle_type = vehicle_type_entry.get()

    ref.child(tag).update({  # Cập nhật dữ liệu trong Firebase Realtime Database dựa trên tag
        'Name': name,
        'money': money,
        'type': vehicle_type
    })
    # Hiển thị hộp thoại thông báo cập nhật thành công
    messagebox.showinfo("Success", "RFID data updated successfully.")

    # Xóa dữ liệu trong các trường nhập liệu sau khi cập nhật thành công
    rfid_tag_entry.delete(0, tk.END)
    name_entry.delete(0, tk.END)
    money_entry.delete(0, tk.END)
    vehicle_type_entry.delete(0, tk.END)

    refresh_user_list()  # Cập nhật danh sách người dùng trên giao diện
    refresh_report()  # Cập nhật báo cáo trên giao diện


def delete_data():
    tag = rfid_tag_entry.get()  # Lấy giá trị từ trường nhập liệu 'rfid_tag_entry'

    # Xóa dữ liệu tương ứng với tag trong Firebase Realtime Database
    ref.child(tag).delete()
    # Hiển thị hộp thoại thông báo xóa thành công
    messagebox.showinfo("Success", "RFID data deleted successfully.")

    # Xóa dữ liệu trong trường nhập liệu 'rfid_tag_entry'
    rfid_tag_entry.delete(0, tk.END)

    refresh_user_list()  # Cập nhật danh sách người dùng trên giao diện
    refresh_report()  # Cập nhật báo cáo trên giao diện


def refresh_user_list():
    # Xóa tất cả các dòng trong danh sách người dùng
    user_listbox.delete(0, tk.END)

    data = ref.get()  # Lấy dữ liệu từ Firebase Realtime Database

    for rfid_tag, rfid_data in data.items():  # Lặp qua từng cặp key-value trong dữ liệu
        # Thêm dòng chứa RFID Tag vào danh sách người dùng
        user_listbox.insert(tk.END, f"RFID Tag: {rfid_tag}")
        # Thêm dòng chứa tên vào danh sách người dùng
        user_listbox.insert(tk.END, f"Name: {rfid_data['Name']}")
        # Thêm dòng chứa số tiền vào danh sách người dùng
        user_listbox.insert(tk.END, f"Money: {rfid_data['money']}")
        # Thêm dòng chứa loại phương tiện vào danh sách người dùng
        user_listbox.insert(tk.END, f"Type: {rfid_data['type']}")
        # Thêm dòng trống để tạo khoảng cách giữa các người dùng trong danh sách
        user_listbox.insert(tk.END, "")


def refresh_report():
    # Xóa tất cả các dòng trong danh sách báo cáo
    report_listbox.delete(0, tk.END)

    # Tham chiếu đến đường dẫn gốc '/' trong Firebase Realtime Database
    ref2 = db.reference('/')

    data = ref2.get()  # Lấy dữ liệu từ Firebase Realtime Database

    if 'report' in data:  # Kiểm tra xem có dữ liệu báo cáo tồn tại trong dữ liệu hay không
        report_data = data['report']  # Lấy dữ liệu báo cáo từ dữ liệu
        # Thêm dòng chữ "Product Report:" vào danh sách báo cáo
        report_listbox.insert(tk.END, "Product Report:")
        # Thêm dòng trống vào danh sách báo cáo
        report_listbox.insert(tk.END, "")

        for vehicle_type, count in report_data.items():  # Lặp qua từng cặp key-value trong dữ liệu báo cáo
            # Chỉ hiển thị báo cáo cho các loại phương tiện nhất định
            if vehicle_type in ["honda", "vin", "yamaha"]:
                report_listbox.insert(
                    tk.END, f"{vehicle_type.capitalize()}: {count}")  # Thêm dòng chứa loại phương tiện và số lượng vào danh sách báo cáo

        for widget in report_frame.winfo_children():  # Xóa các widget con trong khung báo cáo
            widget.destroy()

        # Tạo một đối tượng hình ảnh và trục
        fig, ax = plt.subplots(figsize=(6, 4))
        # Vẽ biểu đồ cột với dữ liệu từ báo cáo
        ax.bar(report_data.keys(), report_data.values())
        ax.set_xlabel("Vehicle Type")  # Đặt nhãn trục x là "Vehicle Type"
        ax.set_ylabel("Count")  # Đặt nhãn trục y là "Count"
        # Đặt tiêu đề cho biểu đồ là "Product Report"
        ax.set_title("Product Report")

        # Tạo một canvas để hiển thị biểu đồ trên khung báo cáo
        canvas = FigureCanvasTkAgg(fig, master=report_frame)
        canvas.draw()
        canvas.get_tk_widget().pack()

    else:
        # Thêm dòng chữ "No report data available." vào danh sách báo cáo nếu không có dữ liệu báo cáo
        report_listbox.insert(tk.END, "No report data available.")


image_path = "D:/HUST/20222/HTN/project-he-thong-nhung/EmbededUI/template.PNG"  # Đường dẫn đến hình ảnh template
image = Image.open(image_path)  # Mở hình ảnh
# Thay đổi kích thước hình ảnh
image = image.resize((1000, 150), Image.ANTIALIAS)
# Tạo đối tượng hình ảnh để sử dụng trong giao diện
photo = ImageTk.PhotoImage(image)
# Tạo nhãn hiển thị hình ảnh trên giao diện
image_label = tk.Label(root, image=photo)
image_label.pack()  # Đặt nhãn hình ảnh vào giao diện

system_name_label = tk.Label(
    root, text="Automatic Battery Collection System", font=("Arial", 24, "bold"))  # Tạo nhãn hiển thị tên hệ thống trên giao diện
# Đặt nhãn tên hệ thống vào giao diện với khoảng cách là 10 pixel
system_name_label.pack(pady=10)

user_list_frame = tk.Frame(root)  # Tạo khung chứa danh sách người dùng
# Đặt khung danh sách người dùng vào giao diện với khoảng cách là 10 pixel
user_list_frame.pack(side=tk.LEFT, padx=10, pady=10)

# Tạo danh sách người dùng
user_listbox = tk.Listbox(user_list_frame, width=40)
user_listbox.pack()  # Đặt danh sách người dùng vào khung

report_frame = tk.Frame(root)  # Tạo khung chứa báo cáo
# Đặt khung báo cáo vào giao diện với khoảng cách là 10 pixel
report_frame.pack(side=tk.LEFT, padx=10, pady=10)

report_listbox = tk.Listbox(report_frame, width=40)  # Tạo danh sách báo cáo
report_listbox.pack()  # Đặt danh sách báo cáo vào khung

data_entry_frame = tk.Frame(root)  # Tạo khung chứa trường nhập liệu
# Đặt khung trường nhập liệu vào giao diện với khoảng cách là 10 pixel
data_entry_frame.pack(side=tk.LEFT, padx=10, pady=10)

rfid_tag_label = tk.Label(
    data_entry_frame, text="RFID Tag:")  # Tạo nhãn "RFID Tag:"
rfid_tag_label.pack()  # Đặt nhãn "RFID Tag:" vào giao diện
# Tạo trường nhập liệu cho RFID Tag
rfid_tag_entry = tk.Entry(data_entry_frame)
rfid_tag_entry.pack()  # Đặt trường nhập liệu RFID Tag vào giao diện

name_label = tk.Label(data_entry_frame, text="Name:")  # Tạo nhãn "Name:"
name_label.pack()  # Đặt nhãn "Name:" vào giao diện
name_entry = tk.Entry(data_entry_frame)  # Tạo trường nhập liệu cho Name
name_entry.pack()  # Đặt trường nhập liệu Name vào giao diện

money_label = tk.Label(data_entry_frame, text="Money:")  # Tạo nhãn "Money:"
money_label.pack()  # Đặt nhãn "Money:" vào giao diện
money_entry = tk.Entry(data_entry_frame)  # Tạo trường nhập liệu cho Money
money_entry.pack()  # Đặt trường nhập liệu Money vào giao diện

vehicle_type_label = tk.Label(
    data_entry_frame, text="Vehicle Type:")  # Tạo nhãn "Vehicle Type:"
vehicle_type_label.pack()  # Đặt nhãn "Vehicle Type:" vào giao diện
# Tạo trường nhập liệu cho Vehicle Type
vehicle_type_entry = tk.Entry(data_entry_frame)
vehicle_type_entry.pack()  # Đặt trường nhập liệu Vehicle Type vào giao diện

# Tạo nút "Insert" và liên kết với hàm insert_data
insert_button = tk.Button(data_entry_frame, text="Insert", command=insert_data)
insert_button.pack()  # Đặt nút "Insert" vào giao diện

# Tạo nút "Edit" và liên kết với hàm edit_data
edit_button = tk.Button(data_entry_frame, text="Edit", command=edit_data)
edit_button.pack()  # Đặt nút "Edit" vào giao diện

# Tạo nút "Delete" và liên kết với hàm delete_data
delete_button = tk.Button(data_entry_frame, text="Delete", command=delete_data)
delete_button.pack()  # Đặt nút "Delete" vào giao diện

refresh_user_list()  # Cập nhật danh sách người dùng trên giao diện
refresh_report()  # Cập nhật báo cáo trên giao diện

root.mainloop()  # Bắt đầu vòng lặp chạy giao diện```
