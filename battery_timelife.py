import tkinter as tk
import pandas as pd
import numpy as np
from sklearn.preprocessing import LabelEncoder, OneHotEncoder
from sklearn.linear_model import LinearRegression

df = pd.read_excel("battery.xlsx")

X = df[['Voltage', 'Remaining Capacity', 'Charge Count', 'Battery Type']]
y = df['Remaining Lifespan']

label_encoder = LabelEncoder()
X['Battery Type'] = label_encoder.fit_transform(X['Battery Type'])

onehot_encoder = OneHotEncoder(sparse=False)
X_encoded = onehot_encoder.fit_transform(X[['Battery Type']])
feature_names = ['Battery Type_' + str(cls) for cls in label_encoder.classes_]
X_encoded = pd.DataFrame(X_encoded, columns=feature_names)
X = pd.concat([X.drop('Battery Type', axis=1), X_encoded], axis=1)

model = LinearRegression()
model.fit(X, y)
 
window = tk.Tk()
window.title("Battery Lifespan Prediction")

window.geometry("400x300")

def predict_lifespan():
    voltage = float(voltage_entry.get())
    remaining_capacity = float(remaining_capacity_entry.get())
    charge_count = float(charge_count_entry.get())
    battery_type = battery_type_entry.get()

    battery_type_encoded = label_encoder.transform([battery_type])
    battery_type_encoded = onehot_encoder.transform([battery_type_encoded])
    input_data = np.array([[voltage, remaining_capacity, charge_count]])
    input_data_encoded = np.concatenate((input_data, battery_type_encoded), axis=1)

    predicted_lifespan = model.predict(input_data_encoded)

    result_label.config(text=f"Thời gian còn lại: {predicted_lifespan[0]:.2f} tuần")

voltage_label = tk.Label(window, text="Voltage:")
voltage_label.pack()
voltage_entry = tk.Entry(window)
voltage_entry.pack()

remaining_capacity_label = tk.Label(window, text="Remaining Capacity:")
remaining_capacity_label.pack()
remaining_capacity_entry = tk.Entry(window)
remaining_capacity_entry.pack()

charge_count_label = tk.Label(window, text="Charge Count:")
charge_count_label.pack()
charge_count_entry = tk.Entry(window)
charge_count_entry.pack()

battery_type_label = tk.Label(window, text="Battery Type:")
battery_type_label.pack()
battery_type_entry = tk.Entry(window)
battery_type_entry.pack()

predict_button = tk.Button(window, text="Predict Lifespan", command=predict_lifespan)
predict_button.pack()

result_label = tk.Label(window, text="Thời gian còn lại: ")
result_label.pack()

window.mainloop()
