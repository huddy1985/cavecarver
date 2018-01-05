/* See 34c3, Maxim Goryachy https://www.youtube.com/watch?v=JMEJCLX2dtw 
 * inside Intel Management Engine, Dal Target de/encrypt 
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Security.Cryptography;

namespace ConsoleApplication1
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 2 && args[0] == "x")
            {
                string data = Def.Decrypt(args[1]);
                System.Console.WriteLine(data);
            }
            else if (args.Length == 3 && args[0] == "z")
            {
                string contents = File.ReadAllText(args[1]);
                byte[] data = Encoding.UTF8.GetBytes(contents);

                Stream stream = Def.EncryptStreamed(args[2]);
                stream.Write(data, 0, data.Length);
                stream.Close();

            } else {
                System.Console.WriteLine("usage:");
                System.Console.WriteLine("decrypt: <crypt> x topoFile.bin");
                System.Console.WriteLine("encrypt: <crypt> z input.txt topoFile.bin");
            }

        }
    }
    public static class Def
    {
        internal const string Salt = "I wandered lonely as a cloud,\r\n            That floats on high o'er vales and hills,\r\n            When all at once I saw a crowd,\r\n            A host of golden daffodils";

        public static string Decrypt(string fileName)
        {
            return Decryptor.DecryptText(File.ReadAllBytes(fileName), "ITP", "I wandered lonely as a cloud,\r\n            That floats on high o'er vales and hills,\r\n            When all at once I saw a crowd,\r\n            A host of golden daffodils");
        }

        public static Stream EncryptStreamed(string fileName)
        {
            FileStream stream = new FileStream(fileName, FileMode.OpenOrCreate, FileAccess.Write);
            return Encryptor.EncryptStream(stream, "ITP", "I wandered lonely as a cloud,\r\n            That floats on high o'er vales and hills,\r\n            When all at once I saw a crowd,\r\n            A host of golden daffodils");
        }
    }

    public static class Decryptor
    {
        public static string DecryptText(byte[] bytes, string password, string salt)
        {
            return Encoding.UTF8.GetString(Decryptor.CreateDecryptor(password, salt).TransformFinalBlock(bytes, 0, bytes.Length));
        }


        private static ICryptoTransform CreateDecryptor(string password, string salt)
        {
            Rfc2898DeriveBytes rfc2898DeriveBytes = new Rfc2898DeriveBytes(password, Encoding.Default.GetBytes(salt + password));
            RijndaelManaged rijndaelManaged = new RijndaelManaged();
            rijndaelManaged.Padding = PaddingMode.PKCS7;
            return rijndaelManaged.CreateDecryptor(rfc2898DeriveBytes.GetBytes(32), rfc2898DeriveBytes.GetBytes(16));
        }
    }

    public static class Encryptor
    {
        public static Stream EncryptStream(Stream stream, string password, string salt)
        {
            ICryptoTransform encryptor = Encryptor.CreateEncryptor(password, salt);
            return (Stream)new CryptoStream(stream, encryptor, CryptoStreamMode.Write);
        }

        private static ICryptoTransform CreateEncryptor(string password, string salt)
        {
            Rfc2898DeriveBytes rfc2898DeriveBytes = new Rfc2898DeriveBytes(password, Encoding.Default.GetBytes(salt + password));
            RijndaelManaged rijndaelManaged = new RijndaelManaged();
            rijndaelManaged.Padding = PaddingMode.PKCS7;
            return rijndaelManaged.CreateEncryptor(rfc2898DeriveBytes.GetBytes(32), rfc2898DeriveBytes.GetBytes(16));
        }
    }

}
