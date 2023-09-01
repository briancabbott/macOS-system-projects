/* <!-- When the user scrolls down from the top of the document, show the button -->
<!-- When user clicks on button go to id=top -->
<!-- include : 
<link rel=stylesheet href=top.css type=text/css>  

and

<script src=top.js></script>                                 

after body tag -->

*/
        window.onscroll = function () { scrollFunction() }; 
        function scrollFunction() {     // hide or show button    ADD to EVERY page so only shows up on big pages!
            if (document.body.scrollTop > 400 || document.documentElement.scrollTop > 400)  // Safari? firefox? chrome? Ms explorer?
                  { document.getElementById("topBtn").style.display="block"; }
             else { document.getElementById("topBtn").style.display="none"; }      <!-- DGG -->
                                    }

        // When the user clicks on the button, scroll to the location 
        function topFunction() {
            //document.body.scrollTop = 0;
            //document.documentElement.scrollTop = 0;
            var scroll_element = document.getElementById("top");
            scroll_element.scrollIntoView();
                                }
document.write('<button onclick="topFunction()" id=topBtn title="Go to top "><b>top</b></button> <a id=top>');

