import argparse
import string
import sys

TEMPLATE = '''<html>
<head>
<meta http-equiv="refresh" content="0; url=${EDN_URI}">
<script type="text/javascript">
window.location.replace("${EDN_URI}");
</script>
</head>

<body>
</body>

</html>
'''

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("uri", help="The URI returned by the EDN")
    
    parser.add_argument(
        "-o",
        help="The output file. If not present stdout is used",
        type=argparse.FileType('w'),
        default=sys.stdout
    )

    args, unkargs = parser.parse_known_args()

    s = string.Template(TEMPLATE).safe_substitute({
        "EDN_URI": args.uri
    })

    args.o.write(s)


if __name__ == "__main__":
    main()
