package com.tencent.matrix.plugin.extension

class MatrixExtension {
    String clientVersion
    String uuid
    String output

    MatrixExtension() {
        clientVersion = ""
        uuid = ""
        output = ""
    }

    @Override
    String toString() {
        """| clientVersion = ${clientVersion}
           | uuid = ${uuid}
        """.stripMargin()
    }
}