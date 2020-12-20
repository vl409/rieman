#!/bin/sh

if [ $# -lt 1 ]; then
    echo "$0: converts rieman XML config into plain text .conf format"
    echo "Usage: $0 <config.xml>"
    exit 1
fi

xsltproc -V > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "xsltproc is not found, exiting"
    exit 1
fi

xsltproc - $1 << EOF |
<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="text" indent="yes"/>
<xsl:strip-space elements="*"/>

<xsl:template match="/rieman-skin">
<xsl:text># rieman skin: </xsl:text><xsl:value-of select="/rieman-skin/@name"/><xsl:text>

</xsl:text>
<xsl:call-template name="file"/>
</xsl:template>

<xsl:template name="file">
<xsl:for-each select="*">
<xsl:call-template name="section"/>
</xsl:for-each>
</xsl:template>

<xsl:template name="section">
<xsl:if test='name(.)!="colors"'>
<xsl:for-each select="*">
<xsl:call-template name="prop_container"/>
</xsl:for-each>
</xsl:if>
</xsl:template>

<xsl:template name="prop_container">
<xsl:for-each select="@*">
 <xsl:choose>
<xsl:when test='name()="color"'>

<xsl:variable name='cname' select="."/>
<xsl:variable name="cdef" select="//rieman-skin/colors/colordef[@name=\$cname]/@hex"/>

<xsl:choose>
<xsl:when test="\$cdef != ''">
<xsl:value-of select="name(../..)"/>.<xsl:value-of select="name(..)"/>.<xsl:value-of select="name()"/><xsl:text> </xsl:text><xsl:value-of select="translate(\$cdef,'#','')"/>
<xsl:text>
</xsl:text>
</xsl:when>

<xsl:otherwise>
<xsl:value-of select="name(../..)"/>.<xsl:value-of select="name(..)"/>.<xsl:value-of select="name()"/><xsl:text> </xsl:text><xsl:value-of select="translate(.,'#','')"/>
</xsl:otherwise>
</xsl:choose>

</xsl:when>
<xsl:otherwise>
<xsl:value-of select="name(../..)"/>.<xsl:value-of select="name(..)"/>.<xsl:value-of select="name()"/><xsl:text> </xsl:text><xsl:value-of select="translate(.,'#','')"/>
<xsl:text>
</xsl:text>
</xsl:otherwise>
</xsl:choose>
</xsl:for-each>
<xsl:text>
</xsl:text>
</xsl:template>


<xsl:template match="/rieman-conf">
    <xsl:text># rieman config

</xsl:text>

<xsl:call-template name="conf_file"/>
</xsl:template>

<xsl:template name="conf_file">

<xsl:for-each select="geometry"><xsl:call-template name="global_prop"/></xsl:for-each>
<xsl:for-each select="layout"><xsl:call-template name="global_prop"/></xsl:for-each>

<xsl:for-each select="appearance"><xsl:call-template name="section"/></xsl:for-each>
<xsl:for-each select="window"><xsl:call-template name="section"/></xsl:for-each>
<xsl:for-each select="actions"><xsl:call-template name="section"/></xsl:for-each>

<xsl:for-each select="control"><xsl:call-template name="global_prop"/></xsl:for-each>

</xsl:template>

<xsl:template name="global_prop">
<xsl:for-each select="@*">
<xsl:value-of select="name(..)"/>.<xsl:value-of select="name()"/><xsl:text> </xsl:text><xsl:value-of select="."/>
<xsl:text>
</xsl:text>
</xsl:for-each>
<xsl:text>
</xsl:text>
</xsl:template>


</xsl:stylesheet>
EOF
sed -e 's/\.enable//g' -e 's/\.value//g' -e 's/\.name//g' -e 's/rieman-conf.//g'
