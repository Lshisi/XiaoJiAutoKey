// SPDX-License-Identifier: AGPL-3.0-only
using System;
using System.Text;

internal static class XiaojiLocalInstaller
{
    static bool Approved(string detail, string token)
    {
        if (String.IsNullOrEmpty(token)) return false;
        string expected = token == "new" ? "首次生成" : token;
        return detail.EndsWith("\n指纹：" + expected, StringComparison.OrdinalIgnoreCase);
    }

    public static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(false);
        try {
            if (args.Length == 1 && args[0] == "--trust-status") {
                Console.WriteLine("XIAOJI_TRUST_TOKEN=" + LocalSetup.TrustToken());
                return 0;
            }
            string approved = "";
            if (args.Length == 3 && args[0] == "--install" && args[1] == "--approve-local-trust") {
                approved = args[2];
                if (approved != "new" && (approved.Length != 40 || !Array.TrueForAll(approved.ToCharArray(), Uri.IsHexDigit)))
                    throw new ArgumentException("Invalid local signing approval token.");
            } else if (args.Length != 1 || args[0] != "--install") {
                throw new ArgumentException("Use --trust-status or --install [--approve-local-trust <token>].");
            }
            // A changed certificate state requires a fresh confirmation in the UI.
            var staged = LocalSetup.RunSetup(false, detail => Approved(detail, approved));
            Console.WriteLine("XIAOJI_SIGNED_INF=" + Convert.ToBase64String(Encoding.UTF8.GetBytes(staged.Published.InfPath)));
            return 0;
        } catch (Exception error) {
            Console.Error.WriteLine("xiaoji: " + error.Message);
            return 1;
        }
    }
}
