set -e

echo "Enabling I2C interface..."
sudo raspi-config nonint do_i2c 0
echo "I2C enabled."

echo "Installing required packages..."
sudo apt update
sudo apt install -y sqlite3 libsqlite3-dev i2c-tools libi2c-dev
echo "Packages installed."

EXEC_NAME="humLogger"
EXEC_PATH="/usr/local/bin/$EXEC_NAME"

if [[ ! -f "$EXEC_NAME" ]]; then
	echo "ERROR: $EXEC_NAME not found in current directory!"
	echo "Place your compiled program (humLogger) next to this script."
	exit 1
fi

echo "Copying executable to /use/local/bin..."
sudo cp "$EXEC_NAME" "$EXEC_PATH"
sudo chmod +x "EXEC_PATH"
echo "Executable installed: $EXEC_PATH"

SERVICE_FILE="etc/systemd/system/humidity.service"

echo "Creating systemd service at $SERVICE_FILE..."

sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=HS3003 Humidity Logger Service
After=network.target

[Service]
Type=simple
ExecStart=$EXEC_PATH
Restart=always
RestartSec=5
User=pi
WorkingDirectory=/usr/local/bin
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
echo "Service created."

echo "Reloading systemd"
sudo systemctl daemon-reload

echo "Enabling humidity service..."
sudo systemctl enable humidity.service

echo "Starting humidity service..."
sudo systemctl start humidity.service

echo "Setup complete!"
echo "Check service status with: sudo systemctl satus humidity.service"
echo "Watch logs with: journalctl -u humidity.service -f"

