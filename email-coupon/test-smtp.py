import json
import os



def send_email():
    import smtplib
    from email.mime.text import MIMEText
    from email.mime.multipart import MIMEMultipart

    f = open("user.json")
    data = json.load(f)
    recipient_email = data['email']
    print("send email to ", recipient_email)
    f.close()

    f = open("email-config.json")
    data = json.load(f)
    sender_email = data['sender_email']
    smtp_username = sender_email
    smtp_password = data['sender_pwd']
    f.close()

    # Email configuration
    subject = "Get free coffee from NIDA Hub Smartbin"
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

    # Attach coupou image
    from email.mime.image import MIMEImage
    ImgFileName = "img_coupon.jpg"
    with open(ImgFileName, 'rb') as f:
        img_data = f.read()
    image = MIMEImage(img_data, name=os.path.basename(ImgFileName))
    msg.attach(image)

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



if __name__ == "__main__":
    send_email()
