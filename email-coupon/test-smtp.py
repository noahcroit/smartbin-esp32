import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import json

# URL for video or camera source
f = open("email-config.json")
data = json.load(f)
sender_email = data['sender_email']
recipient_email = data['receiver_email']
smtp_username = sender_email
smtp_password = data['sender_pwd']
f.close()

# Email configuration
subject = "Hello from Python!"
message = "This is your coupon from smartbin. You got 1 free cup of coffee"

# SMTP server configuration for Outlook
smtp_server = "smtp.office365.com"
smtp_port = 587  # For TLS

# Create the email message
msg = MIMEMultipart()
msg['From'] = sender_email
msg['To'] = recipient_email
msg['Subject'] = subject
msg.attach(MIMEText(message, 'plain'))

# Establish a secure SMTP connection
try:
    server = smtplib.SMTP(smtp_server, smtp_port)
    server.starttls()
    server.login(smtp_username, smtp_password)

    # Send the email
    server.sendmail(sender_email, recipient_email, msg.as_string())

    print("Email sent successfully!")
except Exception as e:
    print("Error: Unable to send email.")
    print(e)
finally:
    # Close the SMTP server connection
    server.quit()
