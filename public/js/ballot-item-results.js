$(document).ready(function() {
    $("body").addClass("javascript-enabled");
    var timeouts = {};
    var MESSAGES = {
        "incomplete": '<p class="incomplete">These are incomplete results.</p>',
        "complete": '<p class="complete">These are complete but unofficial results.</p>'
    };

    var select = $("<select/>").append($('<option/>').text("Select a District…").val("#"));
    $("#sidebar nav#nav-results > ol > li").each(function() {
        //TODO: Fix deeper recursion issues and make this more elegant
        recursivelyGenerateSelectFromList($(this), select);
    });
    $(select).change(function() {
        if (0 == $($(this).val()).trigger("scrollTo").length) {
            location.hash = $(this).val();
        }
    });
    $("#sidebar nav#nav-results > ol").replaceWith(select);

    function recursivelyGenerateSelectFromList(listItem, selectOrOptgroup)
    {
        var option = $("<option/>")
            .text($(listItem).find("a").first().text())
            .val($(listItem).find("a").attr("href"));
        selectOrOptgroup.append(option);
        var optGroup = null; 
        $(listItem).find("> ol > li").each(function () {
            if (null == optGroup) {
                optGroup = $("<optgroup/>");
                selectOrOptgroup.append(optGroup);
            }
            recursivelyGenerateSelectFromList($(this), optGroup);
        });
    }

    $("article.partial-results").each(function() {
        var select = $("<select/>").append($('<option/>').text("Select a District…").val("#"));
        $(this).find("article.ballot-item-results").each(function() {
            var option = $("<option/>").text($(this).find("h1").first().text()).val("#" + $(this).attr("id"));
            select.append(option);
            $(this).hide();
        });
        $(select).change(function() {
            $(this).parent("article").find("article.ballot-item-results").hide();
            var id = $(this).val();
            if ("#" != id) {
                $($(id)).show();
                updateBallotItemArticles();
            }
        });
        if ($(select).find("option").length > 1) {
            $(this).find("h1").first().after(select);
        }
    });

    $(".view-election-event article.district-results").children("article").hide();
    updateBallotItemArticles();
    var scrolling = 0;
    $("article.district-results").bind("scrollTo", function(e) {
        var scrollTarget = "#" + $(this).attr("id");
        //TODO: Find a better way to ignore the parent district results article
        $(scrollTarget).parent("article").parent("article").addClass("scrolled-to");
        $("article.district-results:not(.scrolled-to)").children("article").hide();
        $(scrollTarget).parent("article").parent("article").removeClass("scrolled-to");
        $("article" + scrollTarget).children("article").show().end().parent("article").parent("article").show().children("article").show();
        updateBallotItemArticles();
        $("#sidebar nav#nav-results select").val(scrollTarget);
        scrolling++;
        $.smoothScroll({
            scrollTarget: scrollTarget,
            afterScroll: function () {
                if (1 == scrolling) {
                    location.hash = scrollTarget;
                }
                scrolling--;
            }
        });
        // Prevents the event from bubbling up the DOM tree to parent districts
        e.stopPropagation();
    });

    $("article.district-results h1 a").click(function() {
        $($(this).attr("href")).trigger("scrollTo");
        return false;
    });

    $(window).hashchange(function(e) {
        if ("" != location.hash) {
            $(location.hash).trigger("scrollTo");
        }
    });
    $(window).trigger("hashchange");

    $("a.refresh").click(function(e) {
        e.preventDefault();
        updateBallotItemArticles(false);
    });

    function clearTimeoutsByUri(uri) {
        for (timeout in timeouts[uri]) {
            clearTimeout(timeout);
            delete timeouts[uri][timeout];
        }
    }

    function updateBallotItemArticles(cache) {
        $("article.ballot-item-results").each(function(index, element) {
            var uri = $(element).data('link');
            if (!(uri in timeouts)) {
                timeouts[uri] = {};
            }
            //TODO: Default refresh time?
            updateBallotItemArticle(uri, $(element), 60000, cache);
        });
    }

    function updateBallotItemArticle(uri, element, refreshTime, cache) {
        cache = typeof(cache) != 'undefined' ? cache : true;
        if (element.is(':hidden')) {
            clearTimeoutsByUri(uri);
            var timeout = setTimeout(function() {
                updateBallotItemArticle(uri, element, refreshTime);                
            }, refreshTime + 1000);
            timeouts[uri][timeout] = null;
            return;
        }
        $.ajax({
            url: uri,
            dataType: 'json',
            ifModified: true,
            timeout: 30000,
            cache: cache,
            success: function(data, textStatus, jqXHR) {
                if (304 != jqXHR.status) {
                    var templateRow = element.find("thead tr.result").first().clone();
                    templateRow.find("th").replaceWith(function() {
                        return "<td class=\"" + $(this).attr("class") + "\">" + $(this).html() + "</td>";
                    });
                    //TODO: First item isn't a good template
                    var templateListItem = element.find("ol.graph li.result:first").first().clone();
                    templateListItem.removeClass("winner").removeClass("complete");
                    templateListItem.removeAttr("title");
                    templateListItem.removeAttr("style");
                    var tableBody = element.find("tbody").first().empty();
                    var orderedList = element.find("ol.graph").first().empty();
                    //TODO: "complete" should be at overall results level
                    var complete = null;
                    var totalVotes = 0;
                    for (result in data.results) {
                        totalVotes += parseInt(data.results[result].votes);
                        var resultElement = templateRow.clone();
                        var graphResultElement = templateListItem.clone();
                        resultElement.find(".option").contents().filter(function() {
                            return this.nodeType == 3;
                        }).replaceWith(result);
                        graphResultElement.find(".option").text(result);
                        //TODO: Format votes
                        resultElement.find(".votes").text(data.results[result].votes);
                        graphResultElement.find(".votes").text(data.results[result].votes);
                        resultElement.find(".percent").text(data.results[result].percent + "%");
                        graphResultElement.find(".percent").text(data.results[result].percent);
                        graphResultElement.width(data.results[result].percent + "%");
                        graphResultElement.attr(
                            "title",
                            result + ": " + data.results[result].votes + " votes, " + data.results[result].percent + "%"
                        );
                        if (data.results[result].winner) {
                            resultElement.addClass("winner");
                            graphResultElement.addClass("winner");
                        }
                        if (data.results[result].complete) {
                            resultElement.addClass("complete");
                            graphResultElement.addClass("complete");
                            if (null === complete) {
                                complete = true;
                            }
                        } else if (false === data.results[result].complete) {
                            complete = false;
                        }
                        tableBody.append(resultElement);
                        orderedList.append(graphResultElement);
                        bindResultMouseoverAndMouseout(graphResultElement);
                    }
                    if (totalVotes > 0) {
                        element.find("ol.graph").first().show();
                    } else {
                        element.find("ol.graph").first().hide();
                    }
                    element.find("footer .incomplete").first().remove();
                    element.find("footer .complete").first().remove();
                    //TODO: This check should not be necessary
                    if (0 == element.find("footer .partial").first().length) {
                        if (complete) {
                            element.find("footer").first().prepend(MESSAGES.complete);
                        } else if (false === complete) {
                            element.find("footer").first().prepend(MESSAGES.incomplete);
                        }
                    }
                }
                var date = new Date(jqXHR.getResponseHeader("Date"));
                //TODO: Handle timezone difference between server and client
                element.find(".updated").first().text($.format.date(date, "MMM d, yyyy h:mm:ss a"));
                var expires = new Date(jqXHR.getResponseHeader("Expires"));
                var expiration = expires.valueOf() - date.valueOf();
                if (expiration <= 0) {
                    expiration = refreshTime;
                }
                clearTimeoutsByUri(uri);
                var timeout = setTimeout(function() {
                    updateBallotItemArticle(uri, element, expiration);                
                }, expiration + 1000);
                timeouts[uri][timeout] = null;
            },
            error: function(jqXHR, textStatus, errorThrown) {
                switch (jqXHR.status) {
                    case 503:
                        var retryAfter = jqXHR.getResponseHeader("Retry-After");
                        var date = jqXHR.getResponseHeader("Date");
                        var retry = parseInt(retryAfter) * 1000;
                        if (isNaN(retry)) {
                            retry = Date.parse(retryAfter) - Date.parse(date);
                        }
                        if (isNaN(retry)) {
                            retry = refreshTime;
                        }
                        if (retry <= 0) {
                            retry = refreshTime;
                        }
                        clearTimeoutsByUri(uri);
                        var timeout = setTimeout(function() {
                            updateBallotItemArticle(uri, element, retry);                
                        }, retry + 1000);
                        timeouts[uri][timeout] = null;
                        break;
                    case 400:
                    case 403:
                    case 404:
                    case 410:
                        // Do not repeat the request
                        break;
                    default:
                        clearTimeoutsByUri(uri);
                        var timeout = setTimeout(function() {
                            updateBallotItemArticle(uri, element, refreshTime);                
                        }, refreshTime + 1000);
                        timeouts[uri][timeout] = null;
                        break;
                }
            }
        });
    }
    var graphInfo = $("<div class=\"info\" />").appendTo("body");
    bindResultMouseoverAndMouseout($("article.ballot-item-results .graph .result"));
    function bindResultMouseoverAndMouseout(element) {
        element.mouseover(function() {
            showGraphInfo(graphInfo, $(this));
            $(this).removeAttr("title");
        }).mouseout(function() {
            hideGraphInfo(graphInfo);
        });
    }
    function showGraphInfo(graphInfo, result) {
        var option = result.find(".option").text();
        //TODO: Don't hardcode the concatenated labels
        var votes = result.find(".votes").text() + " votes";
        var percent = result.find(".percent").text() + "%";
        graphInfo.show().empty().append("<h1>" + option + "</h1>").append("<p>Votes: " + votes + "</p>").append("<p>Percent: " + percent + "</p>");
        var marker = $("<canvas class=\"marker\" width=\"" + graphInfo.innerWidth() + "\" height=\"" + (graphInfo.innerHeight() * 0.2) + "\" style=\"bottom: " + (graphInfo.innerHeight() * -0.2) + "px;\" />").appendTo(graphInfo);
        drawGraphInfoMarker(marker, graphInfo.css("backgroundColor"));
        placeGraphInfo(graphInfo, marker, result);
    }
    function hideGraphInfo(graphInfo) {
        graphInfo.hide();
    }
    function placeGraphInfo(graphInfo, marker, result) {
        var placeGraphX = (result.offset().left + (-(graphInfo.innerWidth() / 2) + (result.innerWidth() / 2))) + "px";
        var placeGraphY = (result.offset().top - (graphInfo.innerHeight() + marker.innerHeight())) + "px";
        graphInfo.css("top", placeGraphY).css("left", placeGraphX);
    }
    function drawGraphInfoMarker(marker, backgroundColor) {
        var canvas = marker.get(0);
        var canvasWidth = marker.innerWidth();
        var canvasHeight = marker.innerHeight();
        if (canvas.getContext) {
            var ctx = canvas.getContext("2d");
            ctx.fillStyle = backgroundColor;
            ctx.clearRect(0, 0, canvasWidth, canvasHeight);
            ctx.beginPath();
            ctx.moveTo(canvasWidth * 0.45, 0);
            ctx.lineTo(canvasWidth * 0.55, 0);
            ctx.lineTo(canvasWidth * 0.50, canvasHeight);
            ctx.closePath();
            ctx.fill();
        };
    }
    if ($(0 != "#elections.view-ballot-item-results").length) {
        function resizeViewBallotItemResults() {
            var windowHeight = $(window).height();
            var resultsHeight = $("#elections.view-ballot-item-results #results").innerHeight();
            $("#elections.view-ballot-item-results").height(windowHeight);
            $("#elections.view-ballot-item-results #results").css("marginTop", ((windowHeight - resultsHeight) / 2 * 0.85) + "px")
        }
        resizeViewBallotItemResults();
        $(window).resize(function() {
            resizeViewBallotItemResults();
        });
    }
});
